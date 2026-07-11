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

///////////////////////////////////////////////////////////////////////////////////////
// FILE: OptionPreferences.cpp
// Author: Matthew D. Campbell, April 2002
// Description: Saving/Loading of option preferences
///////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/AudioSettings.h"
#include "Common/GameAudio.h"
#include "Common/GameLOD.h"
#include "Common/GlobalData.h"
#include "Common/OptionPreferences.h"

#include "GameClient/ClientInstance.h"
#include "GameClient/LookAtXlat.h"
#include "GameClient/Mouse.h"

#include "GameLogic/ScriptEngine.h"

#include "GameNetwork/IPEnumeration.h"

OptionPreferences::OptionPreferences()
{
	loadFromIniFile();
}

OptionPreferences::~OptionPreferences()
{
}

Bool OptionPreferences::loadFromIniFile()
{
	if (rts::ClientInstance::getInstanceId() > 1u)
	{
		AsciiString fname;
		fname.format("Options_Instance%.2u.ini", rts::ClientInstance::getInstanceId());
		return load(fname);
	}

	return load("Options.ini");
}

WW3D::MultiSampleModeEnum OptionPreferences::getAntiAliasing() const
{
	OptionPreferences::const_iterator it = find("AntiAliasing");
	if (it == end())
		return WW3D::MULTISAMPLE_MODE_NONE;

	WW3D::MultiSampleModeEnum level = (WW3D::MultiSampleModeEnum)atoi(it->second.str());
	level = clamp(WW3D::MULTISAMPLE_MODE_NONE, level, WW3D::MULTISAMPLE_MODE_8X);
	level = highestBit(level);

	return level;
}

TextureFilterClass::TextureFilterMode OptionPreferences::getTextureFilterMode() const
{
	OptionPreferences::const_iterator it = find("TextureFilter");
	if (it == end())
		return TextureFilterClass::TEXTURE_FILTER_BILINEAR;

	return TextureFilterClass::getTextureFilterMode(it->second.str());
}

TextureFilterClass::AnisotropicFilterMode OptionPreferences::getTextureAnisotropyLevel() const
{
	OptionPreferences::const_iterator it = find("AnisotropyLevel");
	if (it == end())
		return TextureFilterClass::TEXTURE_FILTER_ANISOTROPIC_2X;

	TextureFilterClass::AnisotropicFilterMode level = (TextureFilterClass::AnisotropicFilterMode)atoi(it->second.str());
	level = clamp(TextureFilterClass::TEXTURE_FILTER_ANISOTROPIC_2X, level, TextureFilterClass::TEXTURE_FILTER_ANISOTROPIC_16X);
	level = highestBit(level);

	return level;
}

Int OptionPreferences::getCampaignDifficulty()
{
	OptionPreferences::const_iterator it = find("CampaignDifficulty");
	if (it == end())
		return TheScriptEngine->getGlobalDifficulty();

	Int factor = atoi(it->second.str());
	if (factor < DIFFICULTY_EASY)
		factor = DIFFICULTY_EASY;
	if (factor > DIFFICULTY_HARD)
		factor = DIFFICULTY_HARD;

	return factor;
}

void OptionPreferences::setCampaignDifficulty(Int diff)
{
	AsciiString prefString;
	prefString.format("%d", diff);
	(*this)["CampaignDifficulty"] = prefString;
}

UnsignedInt OptionPreferences::getLANIPAddress()
{
	AsciiString selectedIP = (*this)["IPAddress"];
	IPEnumeration IPs;
	EnumeratedIP *IPlist = IPs.getAddresses();
	while (IPlist)
	{
		if (selectedIP.compareNoCase(IPlist->getIPstring()) == 0)
		{
			return IPlist->getIP();
		}
		IPlist = IPlist->getNext();
	}
	return TheGlobalData->m_defaultIP;
}

void OptionPreferences::setLANIPAddress(AsciiString IP)
{
	(*this)["IPAddress"] = IP;
}

void OptionPreferences::setLANIPAddress(UnsignedInt IP)
{
	AsciiString tmp;
	tmp.format("%d.%d.%d.%d", PRINTF_IP_AS_4_INTS(IP));
	(*this)["IPAddress"] = tmp;
}

UnsignedInt OptionPreferences::getOnlineIPAddress()
{
	AsciiString selectedIP = (*this)["GameSpyIPAddress"];
	IPEnumeration IPs;
	EnumeratedIP *IPlist = IPs.getAddresses();
	while (IPlist)
	{
		if (selectedIP.compareNoCase(IPlist->getIPstring()) == 0)
		{
			return IPlist->getIP();
		}
		IPlist = IPlist->getNext();
	}
	return TheGlobalData->m_defaultIP;
}

void OptionPreferences::setOnlineIPAddress(AsciiString IP)
{
	(*this)["GameSpyIPAddress"] = IP;
}

void OptionPreferences::setOnlineIPAddress(UnsignedInt IP)
{
	AsciiString tmp;
	tmp.format("%d.%d.%d.%d", PRINTF_IP_AS_4_INTS(IP));
	(*this)["GameSpyIPAddress"] = tmp;
}

Bool OptionPreferences::getArchiveReplaysEnabled() const
{
	OptionPreferences::const_iterator it = find("ArchiveReplays");
	if (it == end())
		return FALSE;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getAlternateMouseModeEnabled()
{
	OptionPreferences::const_iterator it = find("UseAlternateMouse");
	if (it == end())
		return TheGlobalData->m_useAlternateMouse;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getRightMouseScrollWithAlternateMouseEnabled() const
{
	OptionPreferences::const_iterator it = find("UseRightMouseScrollWithAlternateMouse");
	if (it == end())
		return TheGlobalData->m_useRightMouseScrollWithAlternateMouse;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getRetaliationModeEnabled()
{
	OptionPreferences::const_iterator it = find("Retaliation");
	if (it == end())
		return TheGlobalData->m_clientRetaliationModeEnabled;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getDoubleClickAttackMoveEnabled()
{
	OptionPreferences::const_iterator it = find("UseDoubleClickAttackMove");
	if( it == end() )
		return TheGlobalData->m_doubleClickAttackMove;

	if( stricmp( it->second.str(), "yes" ) == 0 )
		return TRUE;

	return FALSE;
}

Real OptionPreferences::getScrollFactor()
{
	OptionPreferences::const_iterator it = find("ScrollFactor");
	if (it == end())
		return TheGlobalData->m_keyboardDefaultScrollFactor;

	Int factor = atoi(it->second.str());

	// TheSuperHackers @tweak xezon 11/07/2025
	// No longer caps the upper limit to 100, because the options setting can go beyond that.
	// No longer caps the lower limit to 0, because that would mean standstill.
	if (factor < 1)
		factor = 1;

	return factor/100.0f;
}

Bool OptionPreferences::getDrawScrollAnchor()
{
	OptionPreferences::const_iterator it = find("DrawScrollAnchor");
	// TheSuperHackers @info this default is based on the same variable within InGameUi.ini
	if (it == end())
		return FALSE;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getMoveScrollAnchor()
{
	OptionPreferences::const_iterator it = find("MoveScrollAnchor");
	// TheSuperHackers @info this default is based on the same variable within InGameUi.ini
	if (it == end())
		return TRUE;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getCursorCaptureEnabledInWindowedGame() const
{
	OptionPreferences::const_iterator it = find("CursorCaptureEnabledInWindowedGame");
	if (it == end())
		return (CursorCaptureMode_Default & CursorCaptureMode_EnabledInWindowedGame) != 0;

	if (stricmp(it->second.str(), "yes") == 0)
		return TRUE;

	return FALSE;
}

Bool OptionPreferences::getCursorCaptureEnabledInWindowedMenu() const
{
	OptionPreferences::const_iterator it = find("CursorCaptureEnabledInWindowedMenu");
	if (it == end())
		return (CursorCaptureMode_Default & CursorCaptureMode_EnabledInWindowedMenu) != 0;

	if (stricmp(it->second.str(), "yes") == 0)
		return TRUE;

	return FALSE;
}

Bool OptionPreferences::getCursorCaptureEnabledInFullscreenGame() const
{
	OptionPreferences::const_iterator it = find("CursorCaptureEnabledInFullscreenGame");
	if (it == end())
		return (CursorCaptureMode_Default & CursorCaptureMode_EnabledInFullscreenGame) != 0;

	if (stricmp(it->second.str(), "yes") == 0)
		return TRUE;

	return FALSE;
}

Bool OptionPreferences::getCursorCaptureEnabledInFullscreenMenu() const
{
	OptionPreferences::const_iterator it = find("CursorCaptureEnabledInFullscreenMenu");
	if (it == end())
		return (CursorCaptureMode_Default & CursorCaptureMode_EnabledInFullscreenMenu) != 0;

	if (stricmp(it->second.str(), "yes") == 0)
		return TRUE;

	return FALSE;
}

CursorCaptureMode OptionPreferences::getCursorCaptureMode() const
{
	CursorCaptureMode mode = 0;
	mode |= getCursorCaptureEnabledInWindowedGame() ? CursorCaptureMode_EnabledInWindowedGame : 0;
	mode |= getCursorCaptureEnabledInWindowedMenu() ? CursorCaptureMode_EnabledInWindowedMenu : 0;
	mode |= getCursorCaptureEnabledInFullscreenGame() ? CursorCaptureMode_EnabledInFullscreenGame : 0;
	mode |= getCursorCaptureEnabledInFullscreenMenu() ? CursorCaptureMode_EnabledInFullscreenMenu : 0;
	return mode;
}

Bool OptionPreferences::getScreenEdgeScrollEnabledInWindowedApp() const
{
	OptionPreferences::const_iterator it = find("ScreenEdgeScrollEnabledInWindowedApp");
	if (it == end())
		return (ScreenEdgeScrollMode_Default & ScreenEdgeScrollMode_EnabledInWindowedApp) != 0;

	if (stricmp(it->second.str(), "yes") == 0)
		return TRUE;

	return FALSE;
}

Bool OptionPreferences::getScreenEdgeScrollEnabledInFullscreenApp() const
{
	OptionPreferences::const_iterator it = find("ScreenEdgeScrollEnabledInFullscreenApp");
	if (it == end())
		return (ScreenEdgeScrollMode_Default & ScreenEdgeScrollMode_EnabledInFullscreenApp) != 0;

	if (stricmp(it->second.str(), "yes") == 0)
		return TRUE;

	return FALSE;
}

ScreenEdgeScrollMode OptionPreferences::getScreenEdgeScrollMode() const
{
	ScreenEdgeScrollMode mode = 0;
	mode |= getScreenEdgeScrollEnabledInWindowedApp() ? ScreenEdgeScrollMode_EnabledInWindowedApp : 0;
	mode |= getScreenEdgeScrollEnabledInFullscreenApp() ? ScreenEdgeScrollMode_EnabledInFullscreenApp : 0;
	return mode;
}

Bool OptionPreferences::usesSystemMapDir()
{
	OptionPreferences::const_iterator it = find("UseSystemMapDir");
	if (it == end())
		return TRUE;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::saveCameraInReplays()
{
	OptionPreferences::const_iterator it = find("SaveCameraInReplays");
	if (it == end())
		return TRUE;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::useCameraInReplays()
{
	OptionPreferences::const_iterator it = find("UseCameraInReplays");
	if (it == end())
		return TRUE;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getPlayerObserverEnabled() const
{
	OptionPreferences::const_iterator it = find("PlayerObserverEnabled");
	if (it == end())
		return TRUE;

	if (stricmp(it->second.str(), "yes") == 0)
		return TRUE;

	return FALSE;
}

Int OptionPreferences::getIdealStaticGameDetail()
{
	OptionPreferences::const_iterator it = find("IdealStaticGameLOD");
	if (it == end())
		return STATIC_GAME_LOD_UNKNOWN;

	return TheGameLODManager->getStaticGameLODIndex(it->second);
}

Int OptionPreferences::getStaticGameDetail()
{
	OptionPreferences::const_iterator it = find("StaticGameLOD");
	if (it == end())
		return TheGameLODManager->getStaticLODLevel();

	return TheGameLODManager->getStaticGameLODIndex(it->second);
}

Bool OptionPreferences::getSendDelay()
{
	OptionPreferences::const_iterator it = find("SendDelay");
	if (it == end())
		return TheGlobalData->m_firewallSendDelay;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Int OptionPreferences::getFirewallBehavior()
{
	OptionPreferences::const_iterator it = find("FirewallBehavior");
	if (it == end())
		return TheGlobalData->m_firewallBehavior;

	Int behavior = atoi(it->second.str());
	if (behavior < 0)
	{
		behavior = 0;
	}
	return behavior;
}

Short OptionPreferences::getFirewallPortAllocationDelta()
{
	OptionPreferences::const_iterator it = find("FirewallPortAllocationDelta");
	if (it == end()) {
		return TheGlobalData->m_firewallPortAllocationDelta;
	}

	Short delta = atoi(it->second.str());
	return delta;
}

UnsignedShort OptionPreferences::getFirewallPortOverride()
{
	OptionPreferences::const_iterator it = find("FirewallPortOverride");
	if (it == end()) {
		return TheGlobalData->m_firewallPortOverride;
	}

	Int portOverride = atoi(it->second.str());
	if (portOverride < 0 || portOverride > 65535)
		portOverride = 0;
	return portOverride;
}

Bool OptionPreferences::getFirewallNeedToRefresh()
{
	OptionPreferences::const_iterator it = find("FirewallNeedToRefresh");
	if (it == end()) {
		return FALSE;
	}

	Bool retval = FALSE;
	AsciiString str = it->second;
	if (str.compareNoCase("TRUE") == 0) {
		retval = TRUE;
	}
	return retval;
}

AsciiString OptionPreferences::getPreferred3DProvider()
{
	OptionPreferences::const_iterator it = find("3DAudioProvider");
	if (it == end())
		return TheAudio->getAudioSettings()->m_preferred3DProvider[MAX_HW_PROVIDERS];
	return it->second;
}

AsciiString OptionPreferences::getSpeakerType()
{
	OptionPreferences::const_iterator it = find("SpeakerType");
	if (it == end())
		return TheAudio->translateUnsignedIntToSpeakerType(TheAudio->getAudioSettings()->m_defaultSpeakerType2D);
	return it->second;
}

Real OptionPreferences::getSoundVolume()
{
	OptionPreferences::const_iterator it = find("SFXVolume");
	if (it == end())
	{
		Real relative = TheAudio->getAudioSettings()->m_relative2DVolume;
		if( relative < 0 )
		{
			Real scale = 1.0f + relative;
			return TheAudio->getAudioSettings()->m_defaultSoundVolume * 100.0f * scale;
		}
		return TheAudio->getAudioSettings()->m_defaultSoundVolume * 100.0f;
	}

	Real volume = (Real) atof(it->second.str());
	if (volume < 0.0f)
	{
		volume = 0.0f;
	}
	return volume;
}

Real OptionPreferences::get3DSoundVolume()
{
	OptionPreferences::const_iterator it = find("SFX3DVolume");
	if (it == end())
	{
		Real relative = TheAudio->getAudioSettings()->m_relative2DVolume;
		if( relative > 0 )
		{
			Real scale = 1.0f - relative;
			return TheAudio->getAudioSettings()->m_default3DSoundVolume * 100.0f * scale;
		}
		return TheAudio->getAudioSettings()->m_default3DSoundVolume * 100.0f;
	}

	Real volume = (Real) atof(it->second.str());
	if (volume < 0.0f)
	{
		volume = 0.0f;
	}
	return volume;
}

Real OptionPreferences::getSpeechVolume()
{
	OptionPreferences::const_iterator it = find("VoiceVolume");
	if (it == end())
		return TheAudio->getAudioSettings()->m_defaultSpeechVolume * 100.0f;

	Real volume = (Real) atof(it->second.str());
	if (volume < 0.0f)
	{
		volume = 0.0f;
	}
	return volume;
}

Bool OptionPreferences::getCloudShadowsEnabled()
{
	OptionPreferences::const_iterator it = find("UseCloudMap");
	if (it == end())
		return TheGlobalData->m_useCloudMap;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getLightmapEnabled()
{
	OptionPreferences::const_iterator it = find("UseLightMap");
	if (it == end())
		return TheGlobalData->m_useLightMap;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getSmoothWaterEnabled()
{
	OptionPreferences::const_iterator it = find("ShowSoftWaterEdge");
	if (it == end())
		return TheGlobalData->m_showSoftWaterEdge;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getTreesEnabled()
{
	OptionPreferences::const_iterator it = find("ShowTrees");
	if (it == end())
		return TheGlobalData->m_useTrees;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getExtraAnimationsDisabled()
{
	OptionPreferences::const_iterator it = find("ExtraAnimations");
	if (it == end())
		return TheGlobalData->m_useDrawModuleLOD;

	if (stricmp(it->second.str(), "yes") == 0) {
		return FALSE;	//we are enabling extra animations, so disabled LOD
	}
	return TRUE;
}

Bool OptionPreferences::getUseHeatEffects()
{
	OptionPreferences::const_iterator it = find("HeatEffects");
	if (it == end())
		return TheGlobalData->m_useHeatEffects;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getDynamicLODEnabled()
{
	OptionPreferences::const_iterator it = find("DynamicLOD");
	if (it == end())
		return TheGlobalData->m_enableDynamicLOD;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getFPSLimitEnabled()
{
	OptionPreferences::const_iterator it = find("FPSLimit");
	if (it == end())
	{
		// GeneralsX @bugfix Copilot 11/05/2026 Keep the FPS limiter enabled by default when no explicit preference exists.
		return TRUE;
	}

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::get3DShadowsEnabled()
{
	OptionPreferences::const_iterator it = find("UseShadowVolumes");
	if (it == end())
		return TheGlobalData->m_useShadowVolumes;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::get2DShadowsEnabled()
{
	OptionPreferences::const_iterator it = find("UseShadowDecals");
	if (it == end())
		return TheGlobalData->m_useShadowDecals;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Bool OptionPreferences::getBuildingOcclusionEnabled()
{
	OptionPreferences::const_iterator it = find("BuildingOcclusion");
	if (it == end())
		return TheGlobalData->m_enableBehindBuildingMarkers;

	if (stricmp(it->second.str(), "yes") == 0) {
		return TRUE;
	}
	return FALSE;
}

Int OptionPreferences::getParticleCap()
{
	OptionPreferences::const_iterator it = find("MaxParticleCount");
	if (it == end())
		return TheGlobalData->m_maxParticleCount;

	Int factor = (Int) atoi(it->second.str());
	if (factor < 100)	//clamp to at least 100 particles.
		factor = 100;

	return factor;
}

Int OptionPreferences::getTextureReduction()
{
	OptionPreferences::const_iterator it = find("TextureReduction");
	if (it == end())
		return -1;	//unknown texture reduction

	Int factor = (Int) atoi(it->second.str());
	if (factor > 2)	//clamp it.
		factor=2;
	return factor;
}

Real OptionPreferences::getGammaValue()
{
	OptionPreferences::const_iterator it = find("Gamma");
	if (it == end())
		return 50.0f;

	Real gamma = (Real) atoi(it->second.str());
	return gamma;
}

void OptionPreferences::getResolution(Int *xres, Int *yres)
{
	*xres = TheGlobalData->m_xResolution;
	*yres = TheGlobalData->m_yResolution;

	OptionPreferences::const_iterator it = find("Resolution");
	if (it == end())
		return;

	Int selectedXRes,selectedYRes;
	if (sscanf(it->second.str(),"%d%d", &selectedXRes, &selectedYRes) != 2)
		return;

	*xres=selectedXRes;
	*yres=selectedYRes;
}

Real OptionPreferences::getMusicVolume()
{
	OptionPreferences::const_iterator it = find("MusicVolume");
	if (it == end())
		return TheAudio->getAudioSettings()->m_defaultMusicVolume * 100.0f;

	Real volume = (Real) atof(it->second.str());
	if (volume < 0.0f)
	{
		volume = 0.0f;
	}
	return volume;
}

Real OptionPreferences::getMoneyTransactionVolume() const
{
	OptionPreferences::const_iterator it = find("MoneyTransactionVolume");
	if (it == end())
		return TheAudio->getAudioSettings()->m_defaultMoneyTransactionVolume * 100.0f;

	Real volume = (Real) atof(it->second.str());
	if (volume < 0.0f)
		volume = 0.0f;

	return volume;
}

Int OptionPreferences::getNetworkLatencyFontSize()
{
	OptionPreferences::const_iterator it = find("NetworkLatencyFontSize");
	if (it == end())
		return 8;

	Int fontSize = atoi(it->second.str());
	if (fontSize < 0)
	{
		fontSize = 0;
	}
	return fontSize;
}

Int OptionPreferences::getRenderFpsFontSize()
{
	OptionPreferences::const_iterator it = find("RenderFpsFontSize");
	if (it == end())
		return 8;

	Int fontSize = atoi(it->second.str());
	if (fontSize < 0)
	{
		fontSize = 0;
	}
	return fontSize;
}

Int OptionPreferences::getSystemTimeFontSize()
{
	OptionPreferences::const_iterator it = find("SystemTimeFontSize");
	if (it == end())
		return 8;

	Int fontSize = atoi(it->second.str());
	if (fontSize < 0)
	{
		fontSize = 0;
	}
	return fontSize;
}

Int OptionPreferences::getGameTimeFontSize()
{
	OptionPreferences::const_iterator it = find("GameTimeFontSize");
	if (it == end())
		return 8;

	Int fontSize = atoi(it->second.str());
	if (fontSize < 0)
	{
		fontSize = 0;
	}
	return fontSize;
}

Int OptionPreferences::getPlayerInfoListFontSize()
{
	OptionPreferences::const_iterator it = find("PlayerInfoListFontSize");
	if (it == end())
		return 8;

	Int fontSize = atoi(it->second.str());
	if (fontSize < 0)
	{
		fontSize = 0;
	}
	return fontSize;
}

Real OptionPreferences::getResolutionFontAdjustment()
{
	OptionPreferences::const_iterator it = find("ResolutionFontAdjustment");
	if (it == end())
		return -1.0f;

	Real fontScale = (Real)atof(it->second.str()) / 100.0f;
	if (fontScale < 0.0f)
	{
		fontScale = -1.0f;
	}
	return fontScale;
}

Bool OptionPreferences::getShowMoneyPerMinute() const
{
	OptionPreferences::const_iterator it = find("ShowMoneyPerMinute");
	if (it == end())
		return TheGlobalData->m_showMoneyPerMinute;

	if (stricmp(it->second.str(), "yes") == 0)
	{
		return TRUE;
	}
	return FALSE;
}

Real OptionPreferences::getMaxCameraHeight() const
{
	OptionPreferences::const_iterator it = find("MaxCameraHeight");
	if (it == end())
		return TheGlobalData->m_maxCameraHeight;

	Real val = (Real)atof(it->second.str());
	if (val < 100.0f)
		val = 100.0f;
	if (val > 1000.0f)
		val = 1000.0f;
	return val;
}

Real OptionPreferences::getMinCameraHeight() const
{
	OptionPreferences::const_iterator it = find("MinCameraHeight");
	if (it == end())
		return TheGlobalData->m_minCameraHeight;

	Real val = (Real)atof(it->second.str());
	if (val < 50.0f)
		val = 50.0f;
	if (val > 300.0f)
		val = 300.0f;
	return val;
}

Real OptionPreferences::getCameraPitch() const
{
	OptionPreferences::const_iterator it = find("CameraPitch");
	if (it == end())
		return TheGlobalData->m_cameraPitch;

	Real val = (Real)atof(it->second.str());
	if (val < 20.0f)
		val = 20.0f;
	if (val > 60.0f)
		val = 60.0f;
	return val;
}

Real OptionPreferences::getTerrainDrawDistanceScale() const
{
	OptionPreferences::const_iterator it = find("TerrainDrawDistanceScale");
	if (it == end())
		return TheGlobalData->m_terrainDrawDistanceScale;

	Real val = (Real)atof(it->second.str());
	if (val < 1.0f)
		val = 1.0f;
	if (val > 2.0f)
		val = 2.0f;
	return val;
}

Real OptionPreferences::getGameWindowTransitionSpeedMultiplier() const
{
	OptionPreferences::const_iterator it = find("GameWindowTransitionSpeedMultiplier");
	if (it == end())
		return 1.0f;

	Real speed = (Real) atof(it->second.str());
	return clamp(1.0f, speed, 1000.0f);
}
