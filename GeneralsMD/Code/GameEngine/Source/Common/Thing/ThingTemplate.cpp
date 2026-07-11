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

// FILE: ThingTemplate.cpp ////////////////////////////////////////////////////////////////////////
// Created: Colin Day, April 2001
// Desc:    Thing templates
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_POWER_NAMES								// for PowerNames[]
#define DEFINE_SHADOW_NAMES								// for TheShadowNames[]
#define DEFINE_GEOMETRY_NAMES							// for GeometryNames[]
#define DEFINE_BUILD_COMPLETION_NAMES			// for BuildCompletionNames[]
#define DEFINE_EDITOR_SORTING_NAMES				// for EditorSortingNames[]
#define DEFINE_RADAR_PRIORITY_NAMES				// for RadarPriorityNames[]
#define DEFINE_BUILDABLE_STATUS_NAMES			// for BuildableStatusNames[]

#include "Common/DamageFX.h"
#include "Common/GameAudio.h"
#include "Common/GameCommon.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/MessageStream.h"
#include "Common/Module.h"
#include "Common/ModuleFactory.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ProductionPrerequisite.h"
#include "Common/Radar.h"
#include "Common/RandomValue.h"
#include "Common/Science.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingSort.h"
#include "Common/BitFlagsIO.h"

#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/GameText.h"
#include "GameClient/Image.h"
#include "GameClient/Shadow.h"

#include "GameLogic/Armor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/Powers.h"
#include "GameLogic/Weapon.h"

#include "Common/UnitTimings.h" //Contains the DO_UNIT_TIMINGS define jba.


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
const Int USE_EXP_VALUE_FOR_SKILL_VALUE = -999;


///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

AudioEventRTS ThingTemplate::s_audioEventNoSound;

static void parseKindOfFromINI(INI* ini, void* instance, void *store, const void* userData)
{
	KindOfMaskType::parseFromINI(ini, instance, store, userData);

#if RTS_GENERALS
	KindOfMaskType* kindOf = reinterpret_cast<KindOfMaskType*>(store);

	if (kindOf->test(KINDOF_AIRFIELD))
	{
		// KINDOF_AIRFIELD became KINDOF_FS_AIRFIELD in Zero Hour.
		kindOf->set(KINDOF_FS_AIRFIELD);
	}

	if (kindOf->test(KINDOF_DRONE))
	{
		// KINDOF_DRONE was implicitly KINDOF_NO_SELECT in Generals.
		kindOf->set(KINDOF_NO_SELECT);
	}
#endif
}

/*
	NOTE NOTE NOTE -- s_objectFieldParseTable and s_objectReskinFieldParseTable must be updated in tandem!

	"s_objectReskinFieldParseTable" is intended to be used for parsing the "ObjectReskin" keyword, and
	should only allow you to specify things that affect the appearance of a template.

	The idea is that some units are functionally the same, but different visually; rather than replicate
	all the settings, you can specify it as "this one is like that one, but looks different".

	Thus, currently, the only things in the reskin table are:

		-- DrawModules
		-- DrawModuleData
		-- Geometry

	So: if you add/remove/modify any settings that deal with visual appearance, you *may* want to
	add 'em to the reskin table... but do so VERY CAUTIOUSLY and with careful deliberation (and
	after checking around for other opinions).

*/

// NOTE NOTE NOTE -- s_objectFieldParseTable and s_objectReskinFieldParseTable must be updated in tandem -- see comment above
const FieldParse ThingTemplate::s_objectFieldParseTable[] =
{
	{ "DisplayName",					INI::parseAndTranslateLabel,					nullptr,								offsetof( ThingTemplate, m_displayName ) },
	{ "RadarPriority",				INI::parseByteSizedIndexList,					RadarPriorityNames, offsetof( ThingTemplate, m_radarPriority ) },
	{ "TransportSlotCount",		INI::parseUnsignedByte,								nullptr,		offsetof( ThingTemplate, m_transportSlotCount ) },
	{ "FenceWidth",						INI::parseReal,												nullptr,		offsetof( ThingTemplate, m_fenceWidth ) },
	{ "FenceXOffset",					INI::parseReal,												nullptr,		offsetof( ThingTemplate, m_fenceXOffset ) },
	{ "IsBridge",							INI::parseBool,												nullptr,		offsetof( ThingTemplate, m_isBridge ) },
	{ "ArmorSet",							ThingTemplate::parseArmorTemplateSet, nullptr, 0},
	{ "WeaponSet",						ThingTemplate::parseWeaponTemplateSet,nullptr, 0},
	{ "VisionRange",					INI::parseReal,												nullptr,		offsetof( ThingTemplate, m_visionRange ) },
	{ "ShroudClearingRange",	INI::parseReal,												nullptr,		offsetof( ThingTemplate, m_shroudClearingRange ) },
	{ "ShroudRevealToAllRange",	INI::parseReal,											nullptr,		offsetof( ThingTemplate, m_shroudRevealToAllRange ) },

	{ "PlacementViewAngle",		INI::parseAngleReal,									nullptr,		offsetof( ThingTemplate, m_placementViewAngle ) },

	{ "FactoryExitWidth",			INI::parseReal,												nullptr,		offsetof( ThingTemplate, m_factoryExitWidth ) },
	{ "FactoryExtraBibWidth",	INI::parseReal,												nullptr,		offsetof( ThingTemplate, m_factoryExtraBibWidth ) },

	{ "SkillPointValue",			ThingTemplate::parseIntList,					(void*)LEVEL_COUNT,		offsetof( ThingTemplate, m_skillPointValues ) },
	{ "ExperienceValue",			ThingTemplate::parseIntList,					(void*)LEVEL_COUNT,		offsetof( ThingTemplate, m_experienceValues ) },
	{ "ExperienceRequired",		ThingTemplate::parseIntList,					(void*)LEVEL_COUNT,		offsetof( ThingTemplate, m_experienceRequired ) },
	{ "IsTrainable",					INI::parseBool,												nullptr,									offsetof( ThingTemplate, m_isTrainable ) },
	{ "EnterGuard",						INI::parseBool,												nullptr,									offsetof( ThingTemplate, m_enterGuard ) },
	{ "HijackGuard",					INI::parseBool,												nullptr,									offsetof( ThingTemplate, m_hijackGuard ) },

	{ "Side",									INI::parseAsciiString,								nullptr,	offsetof( ThingTemplate, m_defaultOwningSide ) },

// NOTE NOTE NOTE -- s_objectFieldParseTable and s_objectReskinFieldParseTable must be updated in tandem -- see comment above
	{ "Prerequisites",				ThingTemplate::parsePrerequisites,	nullptr, 0 },
	{ "Buildable",						INI::parseByteSizedIndexList,				BuildableStatusNames, offsetof( ThingTemplate, m_buildable) },
	{ "BuildCost",						INI::parseUnsignedShort,						nullptr,		offsetof( ThingTemplate, m_buildCost ) },
	{ "BuildTime",						INI::parseReal,											nullptr,		offsetof( ThingTemplate, m_buildTime ) },
	{ "RefundValue",					INI::parseUnsignedShort,						nullptr,   offsetof( ThingTemplate, m_refundValue ) },
	{ "BuildCompletion",			INI::parseByteSizedIndexList,				BuildCompletionNames,		offsetof( ThingTemplate, m_buildCompletion ) },
	{ "EnergyProduction",			INI::parseInt,											nullptr,   offsetof( ThingTemplate, m_energyProduction ) },
	{ "EnergyBonus",					INI::parseInt,											nullptr,   offsetof( ThingTemplate, m_energyBonus ) },
	{ "IsForbidden",					INI::parseBool,											nullptr,		offsetof( ThingTemplate, m_isForbidden ) },
	{ "IsPrerequisite",				INI::parseBool,											nullptr,		offsetof( ThingTemplate, m_isPrerequisite ) },
	{ "DisplayColor",					INI::parseColorInt,									nullptr,		offsetof( ThingTemplate, m_displayColor ) },
	{ "EditorSorting",				INI::parseByteSizedIndexList,				EditorSortingNames, offsetof( ThingTemplate, m_editorSorting ) },
	{ "KindOf",								parseKindOfFromINI,									nullptr,		offsetof( ThingTemplate, m_kindof ) },
	{ "CommandSet",						INI::parseAsciiString,							nullptr,		offsetof( ThingTemplate, m_commandSetString ) },
	{ "BuildVariations",			INI::parseAsciiStringVector,				nullptr,		offsetof( ThingTemplate, m_buildVariations ) },

// NOTE NOTE NOTE -- s_objectFieldParseTable and s_objectReskinFieldParseTable must be updated in tandem -- see comment above
	{ "Behavior",							ThingTemplate::parseModuleName,		(const void*)MODULETYPE_BEHAVIOR, offsetof(ThingTemplate, m_behaviorModuleInfo) },
	{ "Body",									ThingTemplate::parseModuleName,		(const void*)999, offsetof(ThingTemplate, m_behaviorModuleInfo) },
	{ "Draw",									ThingTemplate::parseModuleName,		(const void*)MODULETYPE_DRAW, offsetof(ThingTemplate, m_drawModuleInfo) },
	{ "ClientUpdate",					ThingTemplate::parseModuleName,		(const void*)MODULETYPE_CLIENT_UPDATE, offsetof(ThingTemplate, m_clientUpdateModuleInfo) },
// NOTE NOTE NOTE -- s_objectFieldParseTable and s_objectReskinFieldParseTable must be updated in tandem -- see comment above

	{ "SelectPortrait",					INI::parseAsciiString,	nullptr,		offsetof( ThingTemplate, m_selectedPortraitImageName ) },
	{ "ButtonImage",						INI::parseAsciiString,	nullptr,		offsetof( ThingTemplate, m_buttonImageName ) },

	//Code renderer handles these states now.
	//{ "InventoryImageEnabled",	INI::parseAsciiString,	nullptr,		offsetof( ThingTemplate, m_inventoryImage[ INV_IMAGE_ENABLED ] ) },
	//{ "InventoryImageDisabled",	INI::parseAsciiString,	nullptr,		offsetof( ThingTemplate, m_inventoryImage[ INV_IMAGE_DISABLED ] ) },
	//{ "InventoryImageHilite",		INI::parseAsciiString,	nullptr,		offsetof( ThingTemplate, m_inventoryImage[ INV_IMAGE_HILITE ] ) },
	//{ "InventoryImagePushed",		INI::parseAsciiString,	nullptr,		offsetof( ThingTemplate, m_inventoryImage[ INV_IMAGE_PUSHED ] ) },

	{ "UpgradeCameo1",		INI::parseAsciiString,	nullptr,		offsetof( ThingTemplate, m_upgradeCameoUpgradeNames[ 0 ] ) },
	{ "UpgradeCameo2",		INI::parseAsciiString,	nullptr,		offsetof( ThingTemplate, m_upgradeCameoUpgradeNames[ 1 ] ) },
	{ "UpgradeCameo3",		INI::parseAsciiString,	nullptr,		offsetof( ThingTemplate, m_upgradeCameoUpgradeNames[ 2 ] ) },
	{ "UpgradeCameo4",		INI::parseAsciiString,	nullptr,		offsetof( ThingTemplate, m_upgradeCameoUpgradeNames[ 3 ] ) },
	{ "UpgradeCameo5",		INI::parseAsciiString,	nullptr,		offsetof( ThingTemplate, m_upgradeCameoUpgradeNames[ 4 ] ) },

// NOTE NOTE NOTE -- s_objectFieldParseTable and s_objectReskinFieldParseTable must be updated in tandem -- see comment above

	{ "VoiceSelect",					INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceSelect]) },
	{ "VoiceGroupSelect",			INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceGroupSelect]) },
	{ "VoiceMove",						INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceMove]) },
	{ "VoiceAttack",					INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceAttack]) },
	{ "VoiceEnter",						INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceEnter ]) },
	{ "VoiceFear",						INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceFear ]) },
	{ "VoiceSelectElite",			INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceSelectElite ]) },
	{ "VoiceCreated",					INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceCreated]) },
	{ "VoiceTaskUnable",			INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceTaskUnable ]) },
	{ "VoiceTaskComplete",		INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceTaskComplete ]) },
	{ "VoiceMeetEnemy",				INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceMeetEnemy]) },
	{ "VoiceGarrison",				INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceGarrison]) },
#ifdef ALLOW_SURRENDER
	{ "VoiceSurrender",				INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceSurrender]) },
#endif
	{ "VoiceDefect",					INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceDefect]) },
	{ "VoiceAttackSpecial",		INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceAttackSpecial ]) },
	{ "VoiceAttackAir",				INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceAttackAir ]) },
	{ "VoiceGuard",						INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_voiceGuard ]) },
	{ "SoundMoveStart",				INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundMoveStart]) },
	{ "SoundMoveStartDamaged",INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundMoveStartDamaged]) },
	{ "SoundMoveLoop",				INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundMoveLoop]) },
	{ "SoundMoveLoopDamaged",	INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundMoveLoopDamaged]) },
	{ "SoundAmbient",					INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundAmbient ]) },
	{ "SoundAmbientDamaged",	INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundAmbientDamaged ]) },
	{ "SoundAmbientReallyDamaged",INI::parseDynamicAudioEventRTS,	nullptr,offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundAmbientReallyDamaged ]) },
	{ "SoundAmbientRubble",		INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundAmbientRubble]) },
	{ "SoundStealthOn",       INI::parseDynamicAudioEventRTS,  nullptr,  offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundStealthOn ]) },
	{ "SoundStealthOff",      INI::parseDynamicAudioEventRTS,  nullptr,  offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundStealthOff ]) },
	{ "SoundCreated",					INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundCreated ]) },
	{ "SoundOnDamaged",				INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundOnDamaged ]) },
	{ "SoundOnReallyDamaged",	INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundOnReallyDamaged ]) },
	{ "SoundEnter",						INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundEnter ]) },
	{ "SoundExit",						INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundExit ]) },
	{ "SoundPromotedVeteran",	INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundPromotedVeteran ]) },
	{ "SoundPromotedElite",		INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundPromotedElite ]) },
	{ "SoundPromotedHero",		INI::parseDynamicAudioEventRTS,	nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundPromotedHero ]) },
	{ "SoundFallingFromPlane",INI::parseDynamicAudioEventRTS, nullptr,		offsetof( ThingTemplate, m_audioarray.m_audio[TTAUDIO_soundFalling ]) },

	{ "UnitSpecificSounds",		ThingTemplate::parsePerUnitSounds, nullptr, offsetof(ThingTemplate, m_perUnitSounds) },
	{ "UnitSpecificFX",				ThingTemplate::parsePerUnitFX, nullptr, offsetof(ThingTemplate, m_perUnitFX) },
	{ "Scale",								INI::parseReal,						nullptr,		offsetof( ThingTemplate, m_assetScale ) },
	{ "Geometry",							GeometryInfo::parseGeometryType,				nullptr,  offsetof( ThingTemplate, m_geometryInfo ) },
	{ "GeometryMajorRadius",	GeometryInfo::parseGeometryMajorRadius,	nullptr,		offsetof( ThingTemplate, m_geometryInfo ) },
	{ "GeometryMinorRadius",	GeometryInfo::parseGeometryMinorRadius,	nullptr,		offsetof( ThingTemplate, m_geometryInfo ) },
	{ "GeometryHeight",				GeometryInfo::parseGeometryHeight,			nullptr,		offsetof( ThingTemplate, m_geometryInfo ) },
	{ "GeometryIsSmall",			GeometryInfo::parseGeometryIsSmall,			nullptr,		offsetof( ThingTemplate, m_geometryInfo ) },
	{ "Shadow",								INI::parseBitString8,		TheShadowNames,		offsetof( ThingTemplate, m_shadowType ) },
	{ "ShadowSizeX",					INI::parseReal,						nullptr,	offsetof( ThingTemplate, m_shadowSizeX ) },
	{ "ShadowSizeY",					INI::parseReal,						nullptr,	offsetof( ThingTemplate, m_shadowSizeY ) },
	{ "ShadowOffsetX",				INI::parseReal,						nullptr,	offsetof( ThingTemplate, m_shadowOffsetX ) },
	{ "ShadowOffsetY",				INI::parseReal,						nullptr,	offsetof( ThingTemplate, m_shadowOffsetY ) },
	{ "ShadowTexture",				INI::parseAsciiString,		nullptr,	offsetof( ThingTemplate, m_shadowTextureName ) },
	{ "OcclusionDelay",					INI::parseDurationUnsignedInt,		nullptr, offsetof( ThingTemplate, m_occlusionDelay ) },
	{ "AddModule",						ThingTemplate::parseAddModule,			nullptr, 0 },
	{ "RemoveModule",					ThingTemplate::parseRemoveModule,		nullptr, 0 },
	{ "ReplaceModule",				ThingTemplate::parseReplaceModule,	nullptr, 0 },
	{ "InheritableModule",		ThingTemplate::parseInheritableModule,	nullptr, 0 },

  { "OverrideableByLikeKind",		ThingTemplate::OverrideableByLikeKind,	nullptr, 0 },

	{ "Locomotor",						AIUpdateModuleData::parseLocomotorSet, nullptr, 0 },
	{ "InstanceScaleFuzziness",	INI::parseReal,					nullptr, offsetof(ThingTemplate, m_instanceScaleFuzziness ) },
	{ "StructureRubbleHeight",	INI::parseUnsignedByte,					nullptr, offsetof(ThingTemplate, m_structureRubbleHeight ) },
	{ "ThreatValue",						INI::parseUnsignedShort,		nullptr, offsetof(ThingTemplate, m_threatValue ) },
  { "MaxSimultaneousOfType",	ThingTemplate::parseMaxSimultaneous,		nullptr, offsetof(ThingTemplate, m_maxSimultaneousOfType ) },
  { "MaxSimultaneousLinkKey",	NameKeyGenerator::parseStringAsNameKeyType,		nullptr, offsetof(ThingTemplate, m_maxSimultaneousLinkKey ) },
	{ "CrusherLevel",					INI::parseUnsignedByte,			nullptr, offsetof( ThingTemplate, m_crusherLevel ) },
	{ "CrushableLevel",				INI::parseUnsignedByte,			nullptr, offsetof( ThingTemplate, m_crushableLevel ) },

	{ nullptr, nullptr, nullptr, 0 }

};
// NOTE NOTE NOTE -- s_objectFieldParseTable and s_objectReskinFieldParseTable must be updated in tandem -- see comment above

// NOTE NOTE NOTE -- s_objectFieldParseTable and s_objectReskinFieldParseTable must be updated in tandem -- see comment above
const FieldParse ThingTemplate::s_objectReskinFieldParseTable[] =
{
	{ "Draw",									ThingTemplate::parseModuleName,		(const void*)MODULETYPE_DRAW, offsetof(ThingTemplate, m_drawModuleInfo) },

	{ "Geometry",							GeometryInfo::parseGeometryType,				nullptr,  offsetof( ThingTemplate, m_geometryInfo ) },
	{ "GeometryMajorRadius",	GeometryInfo::parseGeometryMajorRadius,	nullptr,		offsetof( ThingTemplate, m_geometryInfo ) },
	{ "GeometryMinorRadius",	GeometryInfo::parseGeometryMinorRadius,	nullptr,		offsetof( ThingTemplate, m_geometryInfo ) },
	{ "GeometryHeight",				GeometryInfo::parseGeometryHeight,			nullptr,		offsetof( ThingTemplate, m_geometryInfo ) },
	{ "GeometryIsSmall",			GeometryInfo::parseGeometryIsSmall,			nullptr,		offsetof( ThingTemplate, m_geometryInfo ) },
	{ "FenceWidth",						INI::parseReal,													nullptr,		offsetof( ThingTemplate, m_fenceWidth ) },
	{ "FenceXOffset",					INI::parseReal,													nullptr,		offsetof( ThingTemplate, m_fenceXOffset ) },

  // Needed to avoid some cheats with the scud storm rebuild hole
  { "MaxSimultaneousOfType",	ThingTemplate::parseMaxSimultaneous,		nullptr, offsetof(ThingTemplate, m_maxSimultaneousOfType ) },
  { "MaxSimultaneousLinkKey",	NameKeyGenerator::parseStringAsNameKeyType,		nullptr, offsetof(ThingTemplate, m_maxSimultaneousLinkKey ) },

	{ nullptr, nullptr, nullptr, 0 }

};
// NOTE NOTE NOTE -- s_objectFieldParseTable and s_objectReskinFieldParseTable must be updated in tandem -- see comment above

// ------------------------------------------------------------------------------------------------
/** See if the tag string is present in any of the module info entries here */
// ------------------------------------------------------------------------------------------------
const ModuleInfo::Nugget *ModuleInfo::getNuggetWithTag( const AsciiString& tag ) const
{

	std::vector< Nugget >::const_iterator it;
	for( it = m_info.begin(); it != m_info.end(); ++it )
		if( (*it).m_moduleTag == tag )
			return &(*it);

	// no match
	return nullptr;

}

// ------------------------------------------------------------------------------------------------
/** Add this module info to the thing template */
// ------------------------------------------------------------------------------------------------
void ModuleInfo::addModuleInfo(ThingTemplate *thingTemplate,
															 const AsciiString& name,
															 const AsciiString& moduleTag,
															 const ModuleData* data,
															 Int interfaceMask,
															 Bool inheritable,
                               Bool overrideableByLikeKind)
{

	//
	// there must be a module tag present, and it must be unique across all module infos
	// for this thing template
	//
#if defined(RTS_DEBUG)
	// get module info
	const Nugget *nugget;

	nugget = thingTemplate->getBehaviorModuleInfo().getNuggetWithTag( moduleTag );
	if( nugget != nullptr )
	{

		// compare this nugget tag against the tag for the new data we're going to submit
		DEBUG_ASSERTCRASH( nugget->m_moduleTag != moduleTag,
											 ("addModuleInfo - ERROR defining module '%s' on thing template '%s'.  The module '%s' has the tag '%s' which must be unique among all modules for this object, but the tag '%s' is also already on module '%s' within this object.\n\nPlease make unique tag names within an object definition",
												name.str(),
												thingTemplate->getName().str(),
												name.str(),
												moduleTag.str(),
												moduleTag.str(),
												nugget->first.str()) );

		// srj sez: prevent people from ignoring this.
		throw INI_INVALID_DATA;
	}

	nugget = thingTemplate->getDrawModuleInfo().getNuggetWithTag( moduleTag );
	if( nugget != nullptr )
	{

		// compare this nugget tag against the tag for the new data we're going to submit
		DEBUG_ASSERTCRASH( nugget->m_moduleTag != moduleTag,
											 ("addModuleInfo - ERROR defining module '%s' on thing template '%s'.  The module '%s' has the tag '%s' which must be unique among all modules for this object, but the tag '%s' is also already on module '%s' within this object.\n\nPlease make unique tag names within an object definition",
												name.str(),
												thingTemplate->getName().str(),
												name.str(),
												moduleTag.str(),
												moduleTag.str(),
												nugget->first.str()) );

		// srj sez: prevent people from ignoring this.
		throw INI_INVALID_DATA;
	}

	nugget = thingTemplate->getClientUpdateModuleInfo().getNuggetWithTag( moduleTag );
	if( nugget != nullptr )
	{

		// compare this nugget tag against the tag for the new data we're going to submit
		DEBUG_ASSERTCRASH( nugget->m_moduleTag != moduleTag,
											 ("addModuleInfo - ERROR defining module '%s' on thing template '%s'.  The module '%s' has the tag '%s' which must be unique among all modules for this object, but the tag '%s' is also already on module '%s' within this object.\n\nPlease make unique tag names within an object definition",
												name.str(),
												thingTemplate->getName().str(),
												name.str(),
												moduleTag.str(),
												moduleTag.str(),
												nugget->first.str()) );
		// srj sez: prevent people from ignoring this.
		throw INI_INVALID_DATA;
	}

#endif

	m_info.push_back(Nugget(name, moduleTag, data, interfaceMask, inheritable, overrideableByLikeKind));

}

//-------------------------------------------------------------------------------------------------
Bool ModuleInfo::clearModuleDataWithTag(const AsciiString& tagToClear, AsciiString& clearedModuleNameOut)
{
	Bool cleared = false;

	// do NOT clear... we only want to modify this if we return true.
	// if we return false, we should leave this unmodified.
	//clearedModuleNameOut.clear();

	for (std::vector<Nugget>::iterator it = m_info.begin(); it != m_info.end(); /* empty */ )
	{
		if (it->m_moduleTag == tagToClear)
		{
			DEBUG_ASSERTCRASH(!cleared, ("Hmm, multiple clears in ModuleInfo::clearModuleDataWithTag, should this be possible?"));
			clearedModuleNameOut = it->first;
			it = m_info.erase(it);
			cleared = true;
		}
		else
		{
			++it;
		}
	}
	return cleared;
}





//-------------------------------------------------------------------------------------------------
Bool ModuleInfo::clearCopiedFromDefaultEntries(Int interfaceMask, const AsciiString &newName, const ThingTemplate *fullTemplate )
{
  static KindOfMaskType ImmuneToGPSScramblerMask;
  KindOfMaskType &m = ImmuneToGPSScramblerMask;
  m.set(KINDOF_AIRCRAFT);// NO PLANES or helicopters
  m.set(KINDOF_SHRUBBERY);// NO trees or bushes
  m.set(KINDOF_OPTIMIZED_TREE);
  m.set(KINDOF_STRUCTURE);// NO buildings
  m.set(KINDOF_DRAWABLE_ONLY);
  m.set(KINDOF_MOB_NEXUS);
  m.set(KINDOF_IGNORED_IN_GUI);
  m.set(KINDOF_CLEARED_BY_BUILD);
  m.set(KINDOF_DEFENSIVE_WALL);
  m.set(KINDOF_BALLISTIC_MISSILE);
  m.set(KINDOF_SUPPLY_SOURCE);
  m.set(KINDOF_BOAT);
  m.set(KINDOF_INERT);
  m.set(KINDOF_BRIDGE);
  m.set(KINDOF_LANDMARK_BRIDGE);
  m.set(KINDOF_BRIDGE_TOWER);
  Bool disallowed =  fullTemplate->isAnyKindOf( ImmuneToGPSScramblerMask );

  static KindOfMaskType CandidateForGPSScramblerMask;
  CandidateForGPSScramblerMask.set(KINDOF_SCORE);
  CandidateForGPSScramblerMask.set(KINDOF_VEHICLE);
  CandidateForGPSScramblerMask.set(KINDOF_INFANTRY);
  CandidateForGPSScramblerMask.set(KINDOF_PORTABLE_STRUCTURE);
  Bool candidate =  fullTemplate->isAnyKindOf( CandidateForGPSScramblerMask );

  Bool ret = false;

	std::vector<Nugget>::iterator it = m_info.begin();
	while( it != m_info.end() )
	{
		if( (it->interfaceMask & interfaceMask) != 0 && it->copiedFromDefault )
		{
      if ( it->inheritable )
			{
				if( it->m_moduleTag.compare("ModuleTag_DefaultAutoHealBehavior") == 0  && !fullTemplate->isTrainable() )
				{
					// Don't inherit this module if it is entirely useless to us.
          it = m_info.erase( it );
			    ret = true;
				}
				else
				{
					++it;//skip to the next nugget, 'cause we inherit this one
				}
			}
      else if ( it->overrideableByLikeKind)
      {

        AsciiString oldName = it->first;
        if ( oldName == newName  //we will dump this instance, since the INI author requested a specific one of the same class
             || disallowed  // or, we just do not Add these special overrideables to these kinds of templates, so just dump it
             || candidate == FALSE )
        {
          it = m_info.erase( it );
			    ret = true;
        }
        else
			    ++it;//no match, preserve the default instance of this Module for now
      }
      else // just dump this instance of this Module, since one of the same interface mask has been added by caller
      {
        it = m_info.erase( it );
			  ret = true;
      }
    }
    else
			++it;
	}
	return ret;
}








//-------------------------------------------------------------------------------------------------
Bool ModuleInfo::clearAiModuleInfo()
{
	Bool ret = false;

	std::vector<Nugget>::iterator it = m_info.begin();
	while( it != m_info.end() )
	{
		if (it->second->isAiModuleData() )
		{
			it = m_info.erase( it );
			ret = true;
		}
		else
		{
			++it;
		}
	}
	return ret;
}

//-------------------------------------------------------------------------------------------------
void ThingTemplate::parseModuleName(INI* ini, void *instance, void* store, const void* userData)
{
	ThingTemplate* self = (ThingTemplate*)instance;
	ModuleInfo* mi = (ModuleInfo*)store;
	// GeneralsX @build BenderAI 11/02/2026 - Use uintptr_t for pointer-to-int cast (64-bit compat)
	ModuleType type = (ModuleType)(UnsignedInt)(uintptr_t)userData;
	const char* token = ini->getNextToken();
	AsciiString tokenStr = token;

	// get the tag string (it is now required)
	AsciiString moduleTagStr;
	try
	{
		moduleTagStr = ini->getNextToken();
	}
	catch( ... )
	{

		DEBUG_CRASH(( "[LINE: %d - FILE: '%s'] Module tag not found for module '%s' on thing template '%s'.  Module tags are required and must be unique for all modules within an object definition",
									ini->getLineNum(), ini->getFilename().str(),
									tokenStr.str(), self->getName().str() ));
		throw;

	}

	Int interfaceMask;

	// ugh -- special case for "Body".
	if (type == 999)
	{
		type = MODULETYPE_BEHAVIOR;
	// what interface(s) does this module support?
		interfaceMask = TheModuleFactory->findModuleInterfaceMask(tokenStr, type);
		if ((interfaceMask & (MODULEINTERFACE_BODY)) == 0)
		{
			DEBUG_CRASH(("Only Body allowed here"));
			throw INI_INVALID_DATA;
		}
	}
	else
	{
		interfaceMask = TheModuleFactory->findModuleInterfaceMask(tokenStr, type);
		if ((interfaceMask & (MODULEINTERFACE_BODY)) != 0)
		{
			DEBUG_CRASH(("No Body allowed here"));
			throw INI_INVALID_DATA;
		}
	}

	// if we're overriding, we can totally skip over this block
	if (ini->getLoadType() == INI_LOAD_CREATE_OVERRIDES)
	{
		if (self->m_moduleParsingMode == MODULEPARSE_ADD_REMOVE_REPLACE)
		{
			// do nothing, just fall thru
		}
		else
		{
			DEBUG_CRASH(("[LINE: %d - FILE: '%s'] You must use AddModule to add modules in override INI files.",
				ini->getLineNum(), ini->getFilename().str(), self->getName().str()));
			throw INI_INVALID_DATA;
		}
	}
	else
	{

//    if (self->getName().compare("GLAVehicleQuadCannon"))
//      DEBUG_CRASH( ("WE ARE CLEARING DEFAULT MODULES FROM A QUAD CANNON.") );

		self->m_behaviorModuleInfo.clearCopiedFromDefaultEntries(interfaceMask, tokenStr, self );
		self->m_drawModuleInfo.clearCopiedFromDefaultEntries(interfaceMask, tokenStr, self );
		self->m_clientUpdateModuleInfo.clearCopiedFromDefaultEntries(interfaceMask, tokenStr, self );
	}

	if (self->m_moduleParsingMode == MODULEPARSE_ADD_REMOVE_REPLACE
			&& self->m_moduleBeingReplacedName.isNotEmpty()
			&& self->m_moduleBeingReplacedName != tokenStr)
	{
		DEBUG_CRASH(("[LINE: %d - FILE: '%s'] ReplaceModule must replace modules with another module of the same type, but you are attempting to replace a %s with a %s for Object %s.",
			ini->getLineNum(), ini->getFilename().str(), self->m_moduleBeingReplacedName.str(), tokenStr.str(), self->getName().str()));
		throw INI_INVALID_DATA;
	}

	if (self->m_moduleParsingMode == MODULEPARSE_ADD_REMOVE_REPLACE
			&& self->m_moduleBeingReplacedTag.isNotEmpty()
			&& self->m_moduleBeingReplacedTag == moduleTagStr)
	{
		DEBUG_CRASH(("[LINE: %d - FILE: '%s'] ReplaceModule must specify a new, unique tag for the replaced module, but you are not doing so for %s (%s) for Object %s.",
			ini->getLineNum(), ini->getFilename().str(), moduleTagStr.str(), self->m_moduleBeingReplacedName.str(), self->getName().str()));
		throw INI_INVALID_DATA;
	}

	ModuleData* data = TheModuleFactory->newModuleDataFromINI(ini, tokenStr, type, moduleTagStr);

	if (data->isAiModuleData())
	{
		Bool replaced = mi->clearAiModuleInfo();
		if (replaced)
		{
			//Kris: Commented this out for SPAM reasons. Do we really need this?
			//DEBUG_LOG(("replaced an AI for %s!",self->getName().str()));
		}
	}

	Bool inheritable = (self->m_moduleParsingMode == MODULEPARSE_INHERITABLE);
  Bool overrideableByLikeKind = (self->m_moduleParsingMode == MODULEPARSE_OVERRIDEABLE_BY_LIKE_KIND);
	mi->addModuleInfo(self, tokenStr, moduleTagStr, data, interfaceMask, inheritable, overrideableByLikeKind);
}

//-------------------------------------------------------------------------------------------------
void ThingTemplate::parseIntList(INI* ini, void *instance, void* store, const void* userData)
{
	// GeneralsX @build BenderAI 11/02/2026 - Use uintptr_t for pointer-to-int cast (64-bit compat)
	Int numberEntries = (Int)(uintptr_t)userData;
	Int *intList = (Int*)store;

	for( Int intIndex = 0; intIndex < numberEntries; intIndex ++ )
	{
		const char *token = ini->getNextToken();
		intList[intIndex] = ini->scanInt(token);
	}
}

//-------------------------------------------------------------------------------------------------
static void parsePrerequisiteUnit( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	std::vector<ProductionPrerequisite>* v = (std::vector<ProductionPrerequisite>*)instance;

	ProductionPrerequisite prereq;
	Bool orUnitWithPrevious = FALSE;
	for (const char *token = ini->getNextToken(); token; token = ini->getNextTokenOrNull())
	{
		prereq.addUnitPrereq( AsciiString( token ), orUnitWithPrevious );
		orUnitWithPrevious = TRUE;
	}

	v->push_back(prereq);
}

//-------------------------------------------------------------------------------------------------
static void parsePrerequisiteScience( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	std::vector<ProductionPrerequisite>* v = (std::vector<ProductionPrerequisite>*)instance;

	ProductionPrerequisite prereq;
	prereq.addSciencePrereq(INI::scanScience(ini->getNextToken()));

	v->push_back(prereq);
}

//-------------------------------------------------------------------------------------------------
void ThingTemplate::parsePrerequisites( INI* ini, void *instance, void *store, const void* userData )
{
	ThingTemplate* self = (ThingTemplate*)instance;

	static const FieldParse myFieldParse[] =
	{
		{ "Object", parsePrerequisiteUnit, nullptr, 0 },
		{ "Science", parsePrerequisiteScience,	nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};

	if (ini->getLoadType() == INI_LOAD_CREATE_OVERRIDES)
	{
		self->m_prereqInfo.clear();
	}

	ini->initFromINI(&self->m_prereqInfo, myFieldParse);
}

//-------------------------------------------------------------------------------------------Static
static void parseArbitraryFXIntoMap( INI* ini, void *instance, void* /* store */, const void* userData )
{
	PerUnitFXMap* mapFX = (PerUnitFXMap*)instance;
	const char* name = (const char*)userData;
	const char* token = ini->getNextToken();
	const FXList* fxl = TheFXListStore->findFXList(token);	// could be null!
	DEBUG_ASSERTCRASH(fxl != nullptr || stricmp(token, "None") == 0, ("FXList %s not found!",token));
	mapFX->insert(std::make_pair(AsciiString(name), fxl));
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ThingTemplate::parsePerUnitFX( INI* ini, void *instance, void *store, const void *userData )
{
	PerUnitFXMap* fxmap = (PerUnitFXMap*)store;

	fxmap->clear();

	static const FieldParse myFieldParse[] =
	{
		{ nullptr, parseArbitraryFXIntoMap, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};

	ini->initFromINI(fxmap, myFieldParse);
}

//-------------------------------------------------------------------------------------------Static
static void parseArbitrarySoundsIntoMap( INI* ini, void *instance, void* /* store */, const void* userData )
{
	PerUnitSoundMap *mapSounds = (PerUnitSoundMap*) instance;
	const char* name = (const char*)userData;
	const char* token = ini->getNextToken();

	AudioEventRTS a;
	if (token)
		a.setEventName(token);
	mapSounds->insert(std::make_pair(AsciiString(name), a));
}

//-------------------------------------------------------------------------------------------------
/** Parse Additional per unit sounds such as TankTurretMove and TankTurretMoveLoop. */
//-------------------------------------------------------------------------------------------------
void ThingTemplate::parsePerUnitSounds( INI* ini, void *instance, void *store, const void *userData )
{
	PerUnitSoundMap *mapSounds = (PerUnitSoundMap*)store;
	mapSounds->clear();

	static const FieldParse myFieldParse[] =
	{
		{ nullptr, parseArbitrarySoundsIntoMap, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};

	ini->initFromINI(mapSounds, myFieldParse);
}

//-------------------------------------------------------------------------------------------------
/** Parse modules to add to the existing set of modules. */
//-------------------------------------------------------------------------------------------------
void ThingTemplate::parseAddModule(INI *ini, void *instance, void *store, const void *userData)
{
	// don't care about the result.
	ThingTemplate* self = (ThingTemplate*)instance;

	ModuleParseMode oldMode = (ModuleParseMode)self->m_moduleParsingMode;
	if (oldMode != MODULEPARSE_NORMAL)
		throw INI_INVALID_DATA;

	self->m_moduleParsingMode = MODULEPARSE_ADD_REMOVE_REPLACE;

	ini->initFromINI(self, self->getFieldParse());

	self->m_moduleParsingMode = oldMode;
}

//-------------------------------------------------------------------------------------------------
/** Parse modules to remove from the existing set of modules. */
//-------------------------------------------------------------------------------------------------
void ThingTemplate::parseRemoveModule(INI *ini, void *instance, void *store, const void *userData)
{
	ThingTemplate* self = (ThingTemplate*)instance;

	ModuleParseMode oldMode = (ModuleParseMode)self->m_moduleParsingMode;
	if (oldMode != MODULEPARSE_NORMAL)
		throw INI_INVALID_DATA;

	self->m_moduleParsingMode = MODULEPARSE_ADD_REMOVE_REPLACE;

	const char *modToRemove = ini->getNextToken();
	AsciiString removedModuleName;
	Bool removed = self->removeModuleInfo(modToRemove, removedModuleName);
	if (!removed)
	{
		DEBUG_ASSERTCRASH(removed, ("RemoveModule %s was not found for %s. The game will crash now!",modToRemove, self->getName().str()));
		throw INI_INVALID_DATA;
	}

	self->m_moduleParsingMode = oldMode;
}

//-------------------------------------------------------------------------------------------------
/** Replace the existing tagged modules with the new modules. */
//-------------------------------------------------------------------------------------------------
void ThingTemplate::parseReplaceModule(INI *ini, void *instance, void *store, const void *userData)
{
	ThingTemplate* self = (ThingTemplate*)instance;

	ModuleParseMode oldMode = (ModuleParseMode)self->m_moduleParsingMode;
	if (oldMode != MODULEPARSE_NORMAL)
		throw INI_INVALID_DATA;

	self->m_moduleParsingMode = MODULEPARSE_ADD_REMOVE_REPLACE;

	const char *modToRemove = ini->getNextToken();
	AsciiString removedModuleName;
	Bool removed = self->removeModuleInfo(modToRemove, removedModuleName);
	if (!removed)
	{
		DEBUG_CRASH(("[LINE: %d - FILE: '%s'] ReplaceModule %s was not found for %s; cannot continue.",
															ini->getLineNum(), ini->getFilename().str(), modToRemove, self->getName().str()));
		throw INI_INVALID_DATA;
	}

	self->m_moduleBeingReplacedName = removedModuleName;
	self->m_moduleBeingReplacedTag = modToRemove;
	ini->initFromINI(self, self->getFieldParse());
	self->m_moduleBeingReplacedName.clear();
	self->m_moduleBeingReplacedTag.clear();

	self->m_moduleParsingMode = oldMode;
}

//-------------------------------------------------------------------------------------------------
/** mark the module(s) as being "Inheritable". */
//-------------------------------------------------------------------------------------------------
void ThingTemplate::parseInheritableModule(INI *ini, void *instance, void *store, const void *userData)
{
	ThingTemplate* self = (ThingTemplate*)instance;

	ModuleParseMode oldMode = (ModuleParseMode)self->m_moduleParsingMode;
	if (oldMode != MODULEPARSE_NORMAL)
		throw INI_INVALID_DATA;

	self->m_moduleParsingMode = MODULEPARSE_INHERITABLE;

	ini->initFromINI(self, self->getFieldParse());

	self->m_moduleParsingMode = oldMode;
}


//-------------------------------------------------------------------------------------------------
/** mark the module(s) as being "VverrideableByLikeKind". default module will be replaced by any of the exact same class */
//-------------------------------------------------------------------------------------------------
void ThingTemplate::OverrideableByLikeKind(INI *ini, void *instance, void *store, const void *userData)
{
	ThingTemplate* self = (ThingTemplate*)instance;

	ModuleParseMode oldMode = (ModuleParseMode)self->m_moduleParsingMode;
	if (oldMode != MODULEPARSE_NORMAL)
		throw INI_INVALID_DATA;

	self->m_moduleParsingMode = MODULEPARSE_OVERRIDEABLE_BY_LIKE_KIND;

	ini->initFromINI(self, self->getFieldParse());

	self->m_moduleParsingMode = oldMode;
}






//-------------------------------------------------------------------------------------------------
/** Remove the module whose tag matches moduleToRemove. */
//-------------------------------------------------------------------------------------------------
Bool ThingTemplate::removeModuleInfo(const AsciiString& moduleToRemove, AsciiString& clearedModuleNameOut)
{
	Bool removed = false;

	// do NOT clear... we only want to modify this if we return true.
	// if we return false, we should leave this unmodified.
	//clearedModuleNameOut.clear();

	if (m_behaviorModuleInfo.clearModuleDataWithTag(moduleToRemove, clearedModuleNameOut))
	{
		DEBUG_ASSERTCRASH(!removed, ("Hmm, multiple removed in ThingTemplate::removeModuleInfo, should this be possible?"));
		removed = true;
	}
	if (m_drawModuleInfo.clearModuleDataWithTag(moduleToRemove, clearedModuleNameOut))
	{
		DEBUG_ASSERTCRASH(!removed, ("Hmm, multiple removed in ThingTemplate::removeModuleInfo, should this be possible?"));
		removed = true;
	}
	if (m_clientUpdateModuleInfo.clearModuleDataWithTag(moduleToRemove, clearedModuleNameOut))
	{
		DEBUG_ASSERTCRASH(!removed, ("Hmm, multiple removed in ThingTemplate::removeModuleInfo, should this be possible?"));
		removed = true;
	}

	return removed;
}

//-------------------------------------------------------------------------------------------------
/// @todo srj -- move this to another file
void ArmorTemplateSet::parseArmorTemplateSet( INI* ini )
{
	static const FieldParse myFieldParse[] =
	{
		{ "Conditions", ArmorSetFlags::parseFromINI, nullptr, offsetof( ArmorTemplateSet, m_types ) },
		{ "Armor", INI::parseArmorTemplate,	nullptr, offsetof( ArmorTemplateSet, m_template ) },
		{ "DamageFX",	INI::parseDamageFX,	nullptr, offsetof( ArmorTemplateSet, m_fx ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	ini->initFromINI(this, myFieldParse);
}

//-------------------------------------------------------------------------------------------------
void ThingTemplate::parseArmorTemplateSet( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	ThingTemplate* self = (ThingTemplate*)instance;
	if (self->m_armorCopiedFromDefault == TRUE)
	{
		self->m_armorCopiedFromDefault = FALSE;
		self->m_armorTemplateSets.clear();
	}

	ArmorTemplateSet ws;
	ws.parseArmorTemplateSet(ini);
#if defined(RTS_DEBUG)
	if (ini->getLoadType() != INI_LOAD_CREATE_OVERRIDES)
	{
		for (ArmorTemplateSetVector::const_iterator it = self->m_armorTemplateSets.begin(); it != self->m_armorTemplateSets.end(); ++it)
		{
			if (it->getNthConditionsYes(0) == ws.getNthConditionsYes(0))
			{
				DEBUG_CRASH(("dup armorset condition in %s",self->getName().str()));
			}
		}
	}
#endif
	self->m_armorTemplateSets.push_back(ws);
	self->m_armorTemplateSetFinder.clear();
}

//-------------------------------------------------------------------------------------------------
void ThingTemplate::parseWeaponTemplateSet( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ )
{
	ThingTemplate* self = (ThingTemplate*)instance;
	if (self->m_weaponsCopiedFromDefault == TRUE)
	{
		self->m_weaponsCopiedFromDefault = FALSE;
		self->m_weaponTemplateSets.clear();
	}

	WeaponTemplateSet ws;
	ws.parseWeaponTemplateSet(ini, self);
#if defined(RTS_DEBUG)
	if (ini->getLoadType() != INI_LOAD_CREATE_OVERRIDES)
	{
		for (WeaponTemplateSetVector::const_iterator it = self->m_weaponTemplateSets.begin(); it != self->m_weaponTemplateSets.end(); ++it)
		{
			if (it->getNthConditionsYes(0) == ws.getNthConditionsYes(0))
			{
				DEBUG_CRASH(("dup weaponset condition in %s",self->getName().str()));
			}
		}
	}
#endif
	self->m_weaponTemplateSets.push_back(ws);
	self->m_weaponTemplateSetFinder.clear();
}

//-------------------------------------------------------------------------------------------------
// Parse the "maxSimultaneousOfType" keyword
void ThingTemplate::parseMaxSimultaneous(INI *ini, void *instance, void *store, const void *userData)
{
  // Most of the time, this is an UnsignedShort, but sometimes this is the keyword
  // "DeterminedBySuperweaponRestriction"
  const char DETERMINED_BY_SUPERWEAPON_KEYWORD[] = "DeterminedBySuperweaponRestriction";

  ThingTemplate *myTemplate = (ThingTemplate *)instance;
  DEBUG_ASSERTCRASH ( &myTemplate->m_maxSimultaneousOfType == store, ("Bad store passed to parseMaxSimultaneous" ) );

  const char * token = ini->getNextToken();
  if ( stricmp( token, DETERMINED_BY_SUPERWEAPON_KEYWORD ) == 0 )
  {
    myTemplate->m_maxSimultaneousDeterminedBySuperweaponRestriction = true;
    *(UnsignedShort *)store = 0;
  }
  else
  {
    // Copied from parseUnsignedShort
    Int value = INI::scanInt(token);
    if (value < 0 || value > 65535)
    {
      DEBUG_CRASH(("Bad value parseMaxSimultaneous"));
      throw ERROR_BUG;
    }
    *(UnsignedShort *)store = (UnsignedShort)value;
    myTemplate->m_maxSimultaneousDeterminedBySuperweaponRestriction = false;
  }
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ThingTemplate::ThingTemplate() :
	m_geometryInfo(GEOMETRY_SPHERE, FALSE, 1, 1, 1)
{
	m_moduleParsingMode = MODULEPARSE_NORMAL;
	m_reskinnedFrom = nullptr;
	m_radarPriority = RADAR_PRIORITY_INVALID;

	m_nextThingTemplate = nullptr;
	m_transportSlotCount = 0;
	m_fenceWidth = 0;
	m_fenceXOffset = 0;
	m_visionRange = 0.0f;
	m_shroudClearingRange = -1.0f;
	m_shroudRevealToAllRange = -1.0f;

	m_buildCost = 0;
	m_buildTime = 1;
	m_refundValue = 0;
	m_energyProduction = 0;
	m_energyBonus = 0;
	m_buildCompletion = BC_APPEARS_AT_RALLY_POINT;

	for( Int levelIndex = 0; levelIndex < LEVEL_COUNT; levelIndex++ )
	{
		m_experienceValues[levelIndex] = 0;
		m_experienceRequired[levelIndex] = 0;
		// -1 means "same value as experienceValues for that level"
		m_skillPointValues[levelIndex] = USE_EXP_VALUE_FOR_SKILL_VALUE;
	}
	m_isTrainable = FALSE;
	m_enterGuard = FALSE;
	m_hijackGuard = FALSE;

	m_templateID = 0;
	m_kindof = KINDOFMASK_NONE;
	//m_defaultOwningSide = "";	// unnecessary
	m_isBuildFacility = FALSE;
	m_isPrerequisite = FALSE;
	m_placementViewAngle = 0.0f;
	m_factoryExitWidth = 0.0f;
	m_factoryExtraBibWidth = 0.0f;

	m_selectedPortraitImage = nullptr;
	m_buttonImage = nullptr;

	m_shadowType = SHADOW_NONE;
	m_shadowSizeX = 0.0f;
	m_shadowSizeY = 0.0f;
	m_shadowOffsetX = 0.0f;
	m_shadowOffsetY = 0.0f;
	m_occlusionDelay = TheGlobalData->m_defaultOcclusionDelay;

	m_structureRubbleHeight = 0;
	m_instanceScaleFuzziness = 0;
	m_threatValue = 0;
	m_maxSimultaneousOfType = 0;	// unlimited
  m_maxSimultaneousLinkKey = NAMEKEY_INVALID; // Not linked
  m_maxSimultaneousDeterminedBySuperweaponRestriction = false;
	m_crusherLevel = 0;			//Unspecified, this object is unable to crush anything!
	m_crushableLevel = 255; //Unspecified, this object is unable to be crushed by anything!

}

//-------------------------------------------------------------------------------------------------
AIUpdateModuleData *ThingTemplate::friend_getAIModuleInfo()
{
	Int numModInfos = m_behaviorModuleInfo.getCount();
	for (int j = 0; j < numModInfos; ++j)
	{
		if (m_behaviorModuleInfo.getNthData(j) && m_behaviorModuleInfo.getNthData(j)->isAiModuleData())
		{
			return (AIUpdateModuleData *)m_behaviorModuleInfo.friend_getNthData(j);
		}
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ThingTemplate::validateAudio()
{
#if defined(RTS_DEBUG)

	#define AUDIO_TEST(y) \
		if (!get##y()->getEventName().isEmpty() && get##y()->getEventName().compareNoCase("NoSound") != 0) { \
			DEBUG_ASSERTLOG(TheAudio->isValidAudioEvent(get##y()), ("Invalid Sound '%s' in Object '%s'. (%s?)", #y, getName().str(), get##y()->getEventName().str())); \
		}

	AUDIO_TEST(VoiceSelect)
	AUDIO_TEST(VoiceGroupSelect)
	AUDIO_TEST(VoiceMove)
	AUDIO_TEST(VoiceAttack)
	AUDIO_TEST(VoiceEnter)
	AUDIO_TEST(VoiceFear)
	AUDIO_TEST(VoiceSelectElite)
	AUDIO_TEST(VoiceCreated)
	AUDIO_TEST(VoiceNearEnemy)
	AUDIO_TEST(VoiceTaskUnable)
	AUDIO_TEST(VoiceTaskComplete)
	AUDIO_TEST(VoiceMeetEnemy)
	AUDIO_TEST(VoiceGarrison)
#ifdef ALLOW_SURRENDER
	AUDIO_TEST(VoiceSurrender)
#endif
	AUDIO_TEST(VoiceDefect)
	AUDIO_TEST(VoiceAttackSpecial)
	AUDIO_TEST(VoiceAttackAir)
	AUDIO_TEST(VoiceGuard)

	AUDIO_TEST(SoundMoveStart)
	AUDIO_TEST(SoundMoveStartDamaged)
	AUDIO_TEST(SoundMoveLoop)
	AUDIO_TEST(SoundMoveLoopDamaged)
	AUDIO_TEST(SoundAmbient)
	AUDIO_TEST(SoundAmbientDamaged)
	AUDIO_TEST(SoundAmbientReallyDamaged)
	AUDIO_TEST(SoundAmbientRubble)
	AUDIO_TEST(SoundStealthOn)
	AUDIO_TEST(SoundStealthOff)
	AUDIO_TEST(SoundCreated)

	AUDIO_TEST(SoundOnDamaged)
	AUDIO_TEST(SoundOnReallyDamaged)
	AUDIO_TEST(SoundEnter)
	AUDIO_TEST(SoundExit)
	AUDIO_TEST(SoundPromotedVeteran)
	AUDIO_TEST(SoundPromotedElite)
	AUDIO_TEST(SoundPromotedHero)

	#undef AUDIO_TEST

	const PerUnitSoundMap *perUnitSounds = getAllPerUnitSounds();
	if (!perUnitSounds)
	{
		return;
	}

	for (PerUnitSoundMap::const_iterator it = perUnitSounds->begin(); it != perUnitSounds->end(); ++it)
	{
		if (!it->second.getEventName().isEmpty() && it->second.getEventName().compareNoCase("NoSound") != 0)
		{
			DEBUG_ASSERTCRASH(TheAudio->isValidAudioEvent(&it->second),
												("Invalid UnitSpecificSound '%s' in Object '%s'. (%s?)",
												it->first.str(),
												getName().str(),
												it->second.getEventName().str()));
		}
	}
#endif
}

//-------------------------------------------------------------------------------------------------
void ThingTemplate::validate()
{
	if (m_shadowTextureName.isEmpty())
	{
		// no texture given, pick a default
		switch (getTemplateGeometryInfo().getGeomType())
		{
			case GEOMETRY_SPHERE:
			case GEOMETRY_CYLINDER:
				m_shadowTextureName = "shadow";
				break;
			case GEOMETRY_BOX:
				m_shadowTextureName = "shadows";
				break;
		}
	}

	validateAudio();

#if defined(RTS_DEBUG)

	if (getName() == "DefaultThingTemplate")
		return;

	// neutron missile is an old special case....
	if (getName() == "NeutronMissile")
		return;

	// another cheesy special case....
	if (getName() == "FlamethrowerProjectileStream")
		return;

	// drawable-only templates are exempt from further checks.
	if (isKindOf(KINDOF_DRAWABLE_ONLY))
		return;

	// build-variation templates are exempt from further checks.
	if (!m_buildVariations.empty())
		return;

	Bool isImmobile = isKindOf(KINDOF_IMMOBILE);

	if (isKindOf(KINDOF_SHRUBBERY) && !isImmobile)
	{
		DEBUG_CRASH(("SHRUBBERY %s must be marked IMMOBILE!",getName().str()));
	}

	if (isKindOf(KINDOF_STRUCTURE) && !isImmobile)
	{
		DEBUG_CRASH(("Structure %s is not marked immobile, but probably should be -- please fix it. (If we ever add mobile structures, this debug sniffer will need to be revised.)",getName().str()));
	}

	if (isKindOf(KINDOF_STICK_TO_TERRAIN_SLOPE) && !isImmobile)
	{
		DEBUG_CRASH(("item %s is marked STICK_TO_TERRAIN_SLOPE but not IMMOBILE -- please fix it.",getName().str()));
	}

	if (isKindOf(KINDOF_STRUCTURE))
	{
		if (m_armorTemplateSets.empty() || (m_armorTemplateSets.size() == 1 && m_armorTemplateSets[0].getArmorTemplate() == nullptr))
		{
			DEBUG_CRASH(("Structure %s has no armor, but probably should (StructureArmor) -- please fix it.)",getName().str()));
		}
		for (ArmorTemplateSetVector::const_iterator it = m_armorTemplateSets.begin(); it != m_armorTemplateSets.end(); ++it)
		{
			if (it->getDamageFX() == nullptr)
			{
				DEBUG_CRASH(("Structure %s has no ArmorDamageFX, and really should.",getName().str()));
			}
		}
	}

#endif
}

//-------------------------------------------------------------------------------------------------
// copy the guts of that into this, but preserve this' name, id, and list-links.
void ThingTemplate::copyFrom(const ThingTemplate* that)
{
	if (!that)
		return;

	ThingTemplate* next = this->m_nextThingTemplate;
	UnsignedShort id = this->m_templateID;
	AsciiString name = this->m_nameString;

	*this = *that;

	this->m_nextThingTemplate = next;
	this->m_templateID = id;
	this->m_nameString = name;
}

//-------------------------------------------------------------------------------------------------
void ThingTemplate::setCopiedFromDefault()
{
	m_armorCopiedFromDefault = true;
	m_weaponsCopiedFromDefault = true;
	m_behaviorModuleInfo.setCopiedFromDefault(true);
	m_drawModuleInfo.setCopiedFromDefault(true);
	m_clientUpdateModuleInfo.setCopiedFromDefault(true);
}

//-------------------------------------------------------------------------------------------------
ThingTemplate::~ThingTemplate()
{
	// note, we don't need to take any special action for Armor/WeaponSets...
	// though it is just a list of 'raw' pointers, we don't have ownership of 'em,
	// and so we MUST NOT delete them
}

//=============================================================================
void ThingTemplate::resolveNames()
{
	Int i, j;

	//Kris: July 31, 2003
	//NOTE: Make sure that all code in this function supports caching properly. For example,
	//      templates can be partially overridden by map.ini files. When this happens, strings
	//      that have been parsed are looked up, cached, then cleared. The problem is if a string
	//      gets cached, but not overridden, it will be clear the next time we call this function.
	//      so we will want to make sure we don't NULL out cached data if the string is empty. A
	//      concrete example is overriding an object with prerequisites. We just override the portrait.
	//      So the 1st time we call this function, we get the standard template data. During this first
	//      call, the strings are looked up, cached, and cleared. Then we override the portrait in the
	//      map.ini. The next time we call this function, we look up all the strings again. The prereq
	//      names didn't used to check for empty strings so they would NULL out all the previous prereqs
	//      the object had. So be sure to make sure all string lookups don't blindly lookup things -- check
	//      if the string isNotEmpty first!

	for (i = 0; i < m_prereqInfo.size(); i++)
	{
		m_prereqInfo[i].resolveNames();
	}

	const Int MAX_BF = 32;
	const ThingTemplate* tmpls[MAX_BF];
	for (i = 0; i < m_prereqInfo.size(); i++)
	{
		Int count = m_prereqInfo[i].getAllPossibleBuildFacilityTemplates(tmpls, MAX_BF);
		for (j = 0; j < count; j++)
		{
			// casting const away is a little evil, but justified in this case:
			// PropductionPrerequisite should only be allowed 'const' access,
			// but ThingTemplate can muck with stuff with gleeful abandon. (srj)
			if( tmpls[ j ] )
				const_cast<ThingTemplate*>(tmpls[j])->m_isBuildFacility = true;
			// DEBUG_LOG(("BF: %s is a buildfacility for %s",tmpls[j]->m_nameString.str(),this->m_nameString.str()));
		}
	}

	if (isKindOf(KINDOF_COMMANDCENTER)) {
		// Command centers are considered factories. jba.
		m_isBuildFacility = true;
	}

	// keep a pointer to portrait and button image if present for speed later
	if( TheMappedImageCollection )
	{
		if( m_selectedPortraitImageName.isNotEmpty() )
		{
			m_selectedPortraitImage = TheMappedImageCollection->findImageByName( m_selectedPortraitImageName );
			DEBUG_ASSERTCRASH( m_selectedPortraitImage, ("%s is looking for Portrait %s but can't find it. Skipping...", getName().str(), m_buttonImageName.str() ) );
			m_selectedPortraitImageName.clear();	// we're done with this, so nuke it
		}
		if( m_buttonImageName.isNotEmpty() )
		{
			m_buttonImage = TheMappedImageCollection->findImageByName( m_buttonImageName );
			DEBUG_ASSERTCRASH( m_buttonImage, ("%s is looking for ButtonImage %s but can't find it. Skipping...", getName().str(), m_buttonImageName.str() ) );
			m_buttonImageName.clear();	// we're done with this, so nuke it
		}
	}

}

//=============================================================================
#ifdef LOAD_TEST_ASSETS
void ThingTemplate::initForLTA(const AsciiString& name)
{
	m_nameString = name;

	char buffer[1024];
	strlcpy(buffer, name.str(), sizeof(buffer));
	int i=0;
	for (; buffer[i]; i++) {
		if (buffer[i] == '/') {
			i++;
			break;
		}
	}
	m_LTAName = AsciiString(buffer+i);

	m_behaviorModuleInfo.clear();
	m_drawModuleInfo.clear();
	m_clientUpdateModuleInfo.clear();

	AsciiString moduleTag;

	moduleTag.format( "LTA_%sDestroyDie", m_LTAName.str() );
	m_behaviorModuleInfo.addModuleInfo(this, "DestroyDie", moduleTag, TheModuleFactory->newModuleDataFromINI(nullptr, "DestroyDie", MODULETYPE_BEHAVIOR, moduleTag), (MODULEINTERFACE_DIE), false);

	moduleTag.format( "LTA_%sInactiveBody", m_LTAName.str() );
	m_behaviorModuleInfo.addModuleInfo(this, "InactiveBody", moduleTag, TheModuleFactory->newModuleDataFromINI(nullptr, "InactiveBody", MODULETYPE_BEHAVIOR, moduleTag), (MODULEINTERFACE_BODY), false);

	moduleTag.format( "LTA_%sW3DDefaultDraw", m_LTAName.str() );
	m_drawModuleInfo.addModuleInfo(this, "W3DDefaultDraw", moduleTag, TheModuleFactory->newModuleDataFromINI(nullptr, "W3DDefaultDraw", MODULETYPE_DRAW, moduleTag), (MODULEINTERFACE_DRAW), false);

	m_armorCopiedFromDefault = false;
	m_weaponsCopiedFromDefault = false;

	m_kindof = KINDOFMASK_NONE;
	m_assetScale = 1.0f;
	m_instanceScaleFuzziness = 0.0f;	///< tolerance to randomly vary scale per instance
	m_structureRubbleHeight = 0.0f;		// zero means "use global default"
	m_displayName.translate( name );
	m_shadowType = SHADOW_VOLUME;

	m_geometryInfo.set(GEOMETRY_SPHERE, false, 10.0, 10.0, 10.0);

}
#endif


//=============================================================================
const ArmorTemplateSet* ThingTemplate::findArmorTemplateSet(const ArmorSetFlags& t) const
{
  return m_armorTemplateSetFinder.findBestInfo(m_armorTemplateSets, t);
}

//=============================================================================
const WeaponTemplateSet* ThingTemplate::findWeaponTemplateSet(const WeaponSetFlags& t) const
{
  return m_weaponTemplateSetFinder.findBestInfo(m_weaponTemplateSets, t);
}

//-----------------------------------------------------------------------------
// returns true iff we have at least one weaponset that contains a weapon.
// returns false if we have no weaponsets, or they are all empty.
Bool ThingTemplate::canPossiblyHaveAnyWeapon() const
{
	for (WeaponTemplateSetVector::const_iterator it = m_weaponTemplateSets.begin();
					it != m_weaponTemplateSets.end();
					++it)
	{
		if (it->hasAnyWeapons())
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
Int ThingTemplate::getSkillPointValue(Int level) const
{
	Int value = m_skillPointValues[level];

	if (value == USE_EXP_VALUE_FOR_SKILL_VALUE)
		value = getExperienceValue(level);

	return value;
}

//-----------------------------------------------------------------------------
const ThingTemplate *ThingTemplate::getBuildFacilityTemplate( const Player *player ) const
{
	if (getPrereqCount() > 0)
	{
		return m_prereqInfo[0].getExistingBuildFacilityTemplate(player);	// might return null
	}
	else
	{
		return nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
BuildableStatus ThingTemplate::getBuildable() const
{
	BuildableStatus bs;
	if (TheGameLogic && TheGameLogic->findBuildableStatusOverride(this, bs))
		return bs;

	return (BuildableStatus)m_buildable;
}

//-------------------------------------------------------------------------------------------------
const FXList *ThingTemplate::getPerUnitFX(const AsciiString& fxName) const
{
	if (fxName.isEmpty())
	{
		return nullptr;
	}

	PerUnitFXMap::const_iterator it = m_perUnitFX.find(fxName);
	if (it == m_perUnitFX.end())
	{
		DEBUG_CRASH(("Unknown FX name (%s) asked for in ThingTemplate (%s)", fxName.str(), m_nameString.str()));
		return nullptr;
	}

	return (it->second);
}

//-------------------------------------------------------------------------------------------------
const AudioEventRTS *ThingTemplate::getPerUnitSound(const AsciiString& soundName) const
{
	if (soundName.isEmpty())
	{
		return &s_audioEventNoSound;
	}

	PerUnitSoundMap::const_iterator it = m_perUnitSounds.find(soundName);
	if (it == m_perUnitSounds.end())
	{
#ifndef DO_UNIT_TIMINGS
    DEBUG_LOG(("Unknown Audio name (%s) asked for in ThingTemplate (%s).", soundName.str(), m_nameString.str()));
#endif
    return &s_audioEventNoSound;
	}

	return &(it->second);
}

//-------------------------------------------------------------------------------------------------
UnsignedInt ThingTemplate::getMaxSimultaneousOfType() const
{
  if ( m_maxSimultaneousDeterminedBySuperweaponRestriction && TheGameLogic )
  {
    return TheGameLogic->getSuperweaponRestriction();
  }

  return m_maxSimultaneousOfType;
}




//-------------------------------------------------------------------------------------------------
Bool ThingTemplate::isEquivalentTo(const ThingTemplate* tt) const
{
	// sanity
	if (!tt)
		return false;

	// sanity
	if (this == tt)
		return true;

	if (this->getFinalOverride() == tt->getFinalOverride())
		return true;

	// This reskinned from that?
	if (this->m_reskinnedFrom == tt)
		return true;

	// That reskinned from this?
	if (this == tt->m_reskinnedFrom)
		return true;

	// This reskinned from that reskinned from?
	// Kris: added case (chassis 2 compared to chassis 3 -- nullptr possible if not reskinned)
	if( this->m_reskinnedFrom && this->m_reskinnedFrom == tt->m_reskinnedFrom )
		return true;

	// Is this thing a build variation of that thing or vice versa
	Int i;

	Int numVariations = m_buildVariations.size();
	for (i = 0; i < numVariations; ++i)
		if (m_buildVariations[i].compareNoCase(tt->getName()) == 0)
			return true;

	numVariations = tt->m_buildVariations.size();
	for (i = 0; i < numVariations; ++i)
		if (tt->m_buildVariations[i].compareNoCase(getName()) == 0)
			return true;

	// Guess we're not equivalent.
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ThingTemplate::isBuildableItem() const
{
	return (getBuildCost() != 0);
}

//-------------------------------------------------------------------------------------------------
/** NOTE that we're not paying attention to m_override here, instead the portions
	* that retrieve template data values use the get() wrappers, which *DO* pay
	* attention to the override values */
//-------------------------------------------------------------------------------------------------
Int ThingTemplate::calcCostToBuild( const Player* player) const
{
	if (!player)
		return 0;

	// changePercent format is "-.2 equals 20% cheaper"
	Real factionModifier = 1 + player->getProductionCostChangePercent( getName() );
	factionModifier *= player->getProductionCostChangeBasedOnKindOf( m_kindof );
	return getBuildCost() * factionModifier * player->getHandicap()->getHandicap(Handicap::BUILDCOST, this);
}

//-------------------------------------------------------------------------------------------------
/** NOTE that we're not paying attention to m_override here, instead the portions
	* that retrieve template data values use the get() wrappers, which *DO* pay
	* attention to the override values */
//-------------------------------------------------------------------------------------------------
Int ThingTemplate::calcTimeToBuild( const Player* player) const
{
	Int buildTime = getBuildTime() * static_cast<float>(LOGICFRAMES_PER_SECOND);
	buildTime *= player->getHandicap()->getHandicap(Handicap::BUILDTIME, this);

	Real factionModifier = 1 + player->getProductionTimeChangePercent( getName() );
	buildTime *= factionModifier;

#if defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
	if( player->buildsInstantly() )
	{
		buildTime = 1;
	}
#endif

	// Adjust build time based on energy supply.

	Real EnergyPercent = player->getEnergy()->getEnergySupplyRatio();	//I'm at 80% Energy
	if (EnergyPercent > 1.0f)
		EnergyPercent = 1.0f;	// getEnergySupplyRatio() returns a true ratio, but we don't care about excess.
	Real EnergyShort = 1.0f - EnergyPercent;					//so I am 20% short
	EnergyShort *= TheGlobalData->m_LowEnergyPenaltyModifier;	//which is a 40% penalty, or a 10% penalty
	Real penaltyRate = 1.0f - EnergyShort;
	penaltyRate = max(penaltyRate, TheGlobalData->m_MinLowEnergyProductionSpeed);	//bind so 0% does not dead stop you

	if( EnergyPercent < 1.0f )	//and make 99% look like 80% (eg) since most of the time you are down only a little
		penaltyRate = min(penaltyRate, TheGlobalData->m_MaxLowEnergyProductionSpeed);

	if (penaltyRate <= 0.0f)
		penaltyRate = 0.01f;	// Design won't make the minimum 0, they promise

	buildTime /= penaltyRate;//and voila.  Makes sense, designers have control, all is happy.

	//	Multiple build facilities can have an added production bonus.

	if (getBuildCompletion() == BC_APPEARS_AT_RALLY_POINT)
	{
		const ThingTemplate *tmpl = getBuildFacilityTemplate(player);	// could be null if none exist
		Int count = 0;
		if (tmpl)
		{
			player->countObjectsByThingTemplate(1, &tmpl, false, &count);
			Real factoryMult = TheGlobalData->m_MultipleFactory;
			if (factoryMult > 0.0f)
			{
				for(int i=0; i < count - 1; i++)
					buildTime *= factoryMult;
			}
		}
	}

	return(buildTime);
}

//---------------------------------------------------------------------------------------ModuleInfo
//-------------------------------------------------------------------------------------------------
ModuleData* ModuleInfo::friend_getNthData(Int i)
{
	if (i >= 0 && i < m_info.size())
	{
		// This is kinda naughty, but its necessary.
		return const_cast<ModuleData*>(m_info[i].second);
	}
	return nullptr;
}

