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

// FILE: AudioEventRTS.cpp
/*---------------------------------------------------------------------------*/
/* EA Pacific                                                                */
/* Confidential Information	                                                 */
/* Copyright (C) 2001 - All Rights Reserved                                  */
/* DO NOT DISTRIBUTE                                                         */
/*---------------------------------------------------------------------------*/
/* Project:    RTS3                                                          */
/* File name:  AudioEventRTS.cpp                                             */
/* Created:    John K. McDonald, Jr., 3/21/2002                              */
/* Desc:       AudioEventRTS constructors and assignment operator, etc.      */
/* Revision History:                                                         */
/*		3/21/2002 : Initial creation                                           */
/*---------------------------------------------------------------------------*/
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/AudioEventRTS.h"

#include "Common/AudioEventInfo.h"
#include "Common/AudioRandomValue.h"
#include "Common/AudioSettings.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "Common/GameSounds.h"
#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/Registry.h"

#include "GameLogic/GameLogic.h"	// For getObjectByID
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/Object.h"

#include "GameClient/Drawable.h"	// For getPosition
#include "GameClient/GameClient.h"	// For getDrawableByID


//-------------------------------------------------------------------------------------------------
AudioEventRTS::AudioEventRTS()
									: m_eventName(AsciiString::TheEmptyString),
										m_priority(AP_NORMAL),
										m_volume(-1.0),
										m_timeOfDay(TIME_OF_DAY_AFTERNOON),
										m_ownerType(OT_INVALID),
										m_shouldFade(false),
										m_isLogicalAudio(false),
										m_filenameToLoad(AsciiString::TheEmptyString),
										m_eventInfo(nullptr),
										m_playingHandle(0),
										m_killThisHandle(0),
										m_pitchShift(1.0),
										m_volumeShift(0.0),
										m_loopCount(1),
										m_playingAudioIndex(-1),
										m_allCount(0),
										m_playerIndex(-1),
										m_delay(0.0f),
										m_uninterruptible(FALSE)
{
	m_attackName.clear();
	m_decayName.clear();
	m_positionOfAudio.zero();
}

//-------------------------------------------------------------------------------------------------
AudioEventRTS::AudioEventRTS( const AsciiString& eventName )
									: m_eventName(eventName),
										m_priority(AP_NORMAL),
										m_volume(-1.0),
										m_timeOfDay(TIME_OF_DAY_AFTERNOON),
										m_ownerType(OT_INVALID),
										m_shouldFade(false),
										m_isLogicalAudio(false),
										m_filenameToLoad(AsciiString::TheEmptyString),
										m_eventInfo(nullptr),
										m_playingHandle(0),
										m_killThisHandle(0),
										m_pitchShift(1.0),
										m_volumeShift(0.0),
										m_loopCount(1),
										m_playingAudioIndex(-1),
										m_allCount(0),
										m_playerIndex(-1),
										m_delay(0.0f),
										m_uninterruptible(FALSE)
{
	m_attackName.clear();
	m_decayName.clear();
	m_positionOfAudio.zero();
}

//-------------------------------------------------------------------------------------------------
AudioEventRTS::AudioEventRTS( const AsciiString& eventName, ObjectID ownerID )
									: m_eventName(eventName),
										m_priority(AP_NORMAL),
										m_volume(-1.0),
										m_timeOfDay(TIME_OF_DAY_AFTERNOON),
										m_objectID(ownerID),
										m_ownerType(OT_INVALID),
										m_shouldFade(false),
										m_isLogicalAudio(false),
										m_filenameToLoad(AsciiString::TheEmptyString),
										m_eventInfo(nullptr),
										m_playingHandle(0),
										m_killThisHandle(0),
										m_pitchShift(1.0),
										m_volumeShift(0.0),
										m_loopCount(1),
										m_playingAudioIndex(-1),
										m_allCount(0),
										m_playerIndex(-1),
										m_delay(0.0f),
										m_uninterruptible(FALSE)
{
	m_attackName.clear();
	m_decayName.clear();

	if(	m_objectID )
	{
		m_ownerType = OT_Object;
	}
	else
	{
		m_objectID = INVALID_ID;
	}
}

//-------------------------------------------------------------------------------------------------
AudioEventRTS::AudioEventRTS( const AsciiString& eventName, DrawableID drawableID )
									: m_eventName(eventName),
										m_priority(AP_NORMAL),
										m_volume(-1.0),
										m_timeOfDay(TIME_OF_DAY_AFTERNOON),
										m_drawableID(drawableID),
										m_ownerType(OT_INVALID),
										m_shouldFade(false),
										m_isLogicalAudio(false),
										m_filenameToLoad(AsciiString::TheEmptyString),
										m_eventInfo(nullptr),
										m_playingHandle(0),
										m_killThisHandle(0),
										m_pitchShift(1.0),
										m_volumeShift(0.0),
										m_loopCount(1),
										m_playingAudioIndex(-1),
										m_allCount(0),
										m_playerIndex(-1),
										m_delay(0.0f),
										m_uninterruptible(FALSE)
{
	m_attackName.clear();
	m_decayName.clear();

	if( m_drawableID )
	{
		m_ownerType = OT_Drawable;
	}
	else
	{
		m_drawableID = INVALID_DRAWABLE_ID;
	}
}

//-------------------------------------------------------------------------------------------------
AudioEventRTS::AudioEventRTS( const AsciiString& eventName, const Coord3D *positionOfAudio )
									: m_eventName(eventName),
										m_priority(AP_NORMAL),
										m_volume(-1.0),
										m_timeOfDay(TIME_OF_DAY_AFTERNOON),
										m_ownerType(OT_Positional),
										m_shouldFade(false),
										m_isLogicalAudio(false),
										m_filenameToLoad(AsciiString::TheEmptyString),
										m_eventInfo(nullptr),
										m_playingHandle(0),
										m_killThisHandle(0),
										m_pitchShift(1.0),
										m_volumeShift(0.0),
										m_loopCount(1),
										m_playingAudioIndex(-1),
										m_allCount(0),
										m_playerIndex(-1),
										m_delay(0.0f),
										m_uninterruptible(FALSE)
{
	m_positionOfAudio.set( *positionOfAudio );
	m_attackName.clear();
	m_decayName.clear();
}

//-------------------------------------------------------------------------------------------------
AudioEventRTS::AudioEventRTS( const AudioEventRTS& right )
{
	m_filenameToLoad			= right.m_filenameToLoad;
	m_eventInfo						= right.m_eventInfo;
	m_playingHandle				= right.m_playingHandle;
	m_killThisHandle			= right.m_killThisHandle;
	m_eventName						= right.m_eventName;
	m_priority						= right.m_priority;
	m_volume							= right.m_volume;
	m_timeOfDay						= right.m_timeOfDay;
	m_ownerType						= right.m_ownerType;
	m_shouldFade					= right.m_shouldFade;
	m_isLogicalAudio			= right.m_isLogicalAudio;
	m_pitchShift					= right.m_pitchShift;
	m_volumeShift					= right.m_volumeShift;
	m_loopCount						= right.m_loopCount;
	m_playingAudioIndex		= right.m_playingAudioIndex;
	m_allCount						= right.m_allCount;
	m_playerIndex					= right.m_playerIndex;
	m_delay								= right.m_delay;
	m_attackName					= right.m_attackName;
	m_decayName						= right.m_decayName;
	m_portionToPlayNext		= right.m_portionToPlayNext;
	m_uninterruptible			= right.m_uninterruptible;

	if( m_ownerType == OT_Positional || m_ownerType == OT_Dead )
	{
		m_positionOfAudio.set( right.m_positionOfAudio );
	}
	else if( m_ownerType == OT_Drawable )
	{
		m_drawableID = right.m_drawableID;
	}
	else if( m_ownerType == OT_Object )
	{
		m_objectID = right.m_objectID;
	}

}

//-------------------------------------------------------------------------------------------------
AudioEventRTS& AudioEventRTS::operator=( const AudioEventRTS& right )
{
	m_filenameToLoad			= right.m_filenameToLoad;
	m_eventInfo						= right.m_eventInfo;
	m_playingHandle				= right.m_playingHandle;
	m_killThisHandle			= right.m_killThisHandle;
	m_eventName						= right.m_eventName;
	m_priority						= right.m_priority;
	m_volume							= right.m_volume;
	m_timeOfDay						= right.m_timeOfDay;
	m_ownerType						= right.m_ownerType;
	m_shouldFade					= right.m_shouldFade;
	m_isLogicalAudio			= right.m_isLogicalAudio;
	m_pitchShift					= right.m_pitchShift;
	m_volumeShift					= right.m_volumeShift;
	m_loopCount						= right.m_loopCount;
	m_playingAudioIndex		= right.m_playingAudioIndex;
	m_allCount						= right.m_allCount;
	m_playerIndex					= right.m_playerIndex;
	m_delay								= right.m_delay;
	m_attackName					= right.m_attackName;
	m_decayName						= right.m_decayName;
	m_portionToPlayNext		= right.m_portionToPlayNext;
	m_uninterruptible			= right.m_uninterruptible;

	if( m_ownerType == OT_Positional || m_ownerType == OT_Dead )
	{
		m_positionOfAudio.set( right.m_positionOfAudio );
	}
	else if( m_ownerType == OT_Drawable )
	{
		m_drawableID = right.m_drawableID;
	}
	else if( m_ownerType == OT_Object )
	{
		m_objectID = right.m_objectID;
	}
	return *this;

}

//-------------------------------------------------------------------------------------------------
AudioEventRTS::~AudioEventRTS()
{
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setEventName( AsciiString name )
{
	if ((name != m_eventName) && m_eventInfo != nullptr) {
		// Clear out the audio event info, cause its not valid for the new event.
		m_eventInfo = nullptr;
	}

	m_eventName = name;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::generateFilename()
{
	// A Logic Random Value is used because we may ask "How long will it take to play this sound?"
	// In that case, we need the same answer across all pcs.

	if (!m_eventInfo) {
		return;
	}

	m_filenameToLoad = generateFilenamePrefix(m_eventInfo->m_soundType, false);

	Int which = 0;

	if (m_eventInfo->m_soundType == AT_Music || m_eventInfo->m_soundType == AT_Streaming) {
		m_filenameToLoad.concat(m_eventInfo->m_filename);
		adjustForLocalization(m_filenameToLoad);
		return;
	} else {
		if (m_eventInfo->m_sounds.empty()) {
			m_filenameToLoad = AsciiString::TheEmptyString;
			return;
		}



		if (BitIsSet(m_eventInfo->m_control, AC_RANDOM))
		{
			if (m_isLogicalAudio)
			{
				which = GameLogicRandomValueUnchanged(0, m_eventInfo->m_sounds.size() - 1);
			}
			else
			{
				which = GameAudioRandomValue(0, m_eventInfo->m_sounds.size() - 1);
			}

			if (which == m_playingAudioIndex && m_eventInfo->m_sounds.size() > 2)
				which = ( which + 1 ) % m_eventInfo->m_sounds.size();

			m_playingAudioIndex = which;//caching random choice to compare next call

		}
		else
			which = (++m_playingAudioIndex) % m_eventInfo->m_sounds.size();


	}

	m_filenameToLoad.concat(m_eventInfo->m_sounds[which]);
	m_filenameToLoad.concat(generateFilenameExtension(m_eventInfo->m_soundType));
	adjustForLocalization(m_filenameToLoad);

	// Note: Also generate Delay when generating a filename, cause
	// we want delay to apply between every loop of a sound.
	m_delay = GameAudioRandomValueReal(m_eventInfo->m_delayMin, m_eventInfo->m_delayMax);
}

//-------------------------------------------------------------------------------------------------
AsciiString AudioEventRTS::getFilename()
{
	return m_filenameToLoad;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::generatePlayInfo()
{
	m_pitchShift = GameAudioRandomValueReal(m_eventInfo->m_pitchShiftMin, m_eventInfo->m_pitchShiftMax);
	m_volumeShift = GameAudioRandomValueReal(1.0f + m_eventInfo->m_volumeShift, 1.0f);	// volume shifts are between 0 and 1
	m_loopCount = m_eventInfo->m_loopCount;

	m_portionToPlayNext = PP_Attack;
	Int attackSize = m_eventInfo->m_attackSounds.size();
	if (attackSize > 0) {
		m_attackName = generateFilenamePrefix(m_eventInfo->m_soundType, false);
		// needs to be logic because it needs to be the same on all systems.
		Int attackToPlay;
		if (m_isLogicalAudio) {
			attackToPlay = GameLogicRandomValueUnchanged(0, attackSize - 1);
		} else {
			attackToPlay = GameAudioRandomValue(0, attackSize - 1);
		}

		m_attackName.concat(m_eventInfo->m_attackSounds[attackToPlay]);
		m_attackName.concat(generateFilenameExtension(m_eventInfo->m_soundType));
		adjustForLocalization(m_attackName);
	} else {
		m_portionToPlayNext = PP_Sound;
	}

	Int decaySize = m_eventInfo->m_decaySounds.size();
	if (decaySize > 0) {
		m_decayName = generateFilenamePrefix(m_eventInfo->m_soundType, false);
		// needs to be logic because it needs to be the same on all systems.
		Int decayToPlay;
		if (m_isLogicalAudio) {
			decayToPlay = GameLogicRandomValueUnchanged(0, decaySize - 1);
		} else {
			decayToPlay = GameAudioRandomValue(0, decaySize - 1);
		}

		m_decayName.concat(m_eventInfo->m_decaySounds[decayToPlay]);
		m_decayName.concat(generateFilenameExtension(m_eventInfo->m_soundType));
		adjustForLocalization(m_decayName);
	}

	m_isLogicalAudio = FALSE;
}

//-------------------------------------------------------------------------------------------------
Real AudioEventRTS::getPitchShift() const
{
	return m_pitchShift;
}

//-------------------------------------------------------------------------------------------------
Real AudioEventRTS::getVolumeShift() const
{
	return m_volumeShift;
}

//-------------------------------------------------------------------------------------------------
AsciiString AudioEventRTS::getAttackFilename() const
{
	return m_attackName;
}

//-------------------------------------------------------------------------------------------------
AsciiString AudioEventRTS::getDecayFilename() const
{
	return m_decayName;
}

//-------------------------------------------------------------------------------------------------
Real AudioEventRTS::getDelay() const
{
	return m_delay;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::decrementDelay( Real timeToDecrement )
{
	m_delay -= timeToDecrement;
}

//-------------------------------------------------------------------------------------------------
PortionToPlay AudioEventRTS::getNextPlayPortion() const
{
	return m_portionToPlayNext;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::advanceNextPlayPortion()
{
	switch (m_portionToPlayNext)
	{
		case PP_Attack:
			m_portionToPlayNext = PP_Sound;
			break;
		case PP_Sound:
			if (m_eventInfo && BitIsSet(m_eventInfo->m_control, AC_ALL))
			{
				if (m_allCount == m_eventInfo->m_sounds.size()) {
					m_portionToPlayNext = PP_Decay;
				}

				// Advance the all count so that we move to the next sound.
				++m_allCount;
			}
			if (!m_decayName.isEmpty()) {
				m_portionToPlayNext = PP_Decay;
			} else {
				m_portionToPlayNext = PP_Done;
			}
			break;
		case PP_Decay:
			m_portionToPlayNext = PP_Done;
			break;
	}
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setNextPlayPortion( PortionToPlay ptp )
{
	m_portionToPlayNext = ptp;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::decreaseLoopCount()
{
	if (m_loopCount == 1) {
		m_loopCount = -1;
	} else if (m_loopCount > 1) {
		--m_loopCount;
	}
}

//-------------------------------------------------------------------------------------------------
Bool AudioEventRTS::hasMoreLoops() const
{
	return (m_loopCount >= 0);
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setAudioEventInfo( const AudioEventInfo *eventInfo ) const
{
	m_eventInfo = eventInfo;
}

//-------------------------------------------------------------------------------------------------
const AudioEventInfo *AudioEventRTS::getAudioEventInfo() const
{
	if (m_eventInfo) {
		if (m_eventInfo->m_audioName == m_eventName) {
			return m_eventInfo;
		} else {
			m_eventInfo = nullptr;
		}
	}

	return m_eventInfo;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setPlayingHandle( AudioHandle handle )
{
	m_playingHandle = handle;
}

//-------------------------------------------------------------------------------------------------
AudioHandle AudioEventRTS::getPlayingHandle()
{
	return m_playingHandle;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setPosition( const Coord3D *pos )
{
	if (!pos) {
		return;
	}

	if (!(m_ownerType == OT_Positional || m_ownerType == OT_INVALID)) {
		return;
	}

	m_positionOfAudio = *pos;
	m_ownerType = OT_Positional;
}

//-------------------------------------------------------------------------------------------------
const Coord3D* AudioEventRTS::getPosition()
{
	if( m_ownerType != OT_INVALID )
	{
		return &m_positionOfAudio;
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setObjectID( ObjectID objID )
{
	if (!(m_ownerType == OT_Object || m_ownerType == OT_INVALID)) {
		return;
	}

	m_objectID = objID;
	m_ownerType = OT_Object;
}

//-------------------------------------------------------------------------------------------------
ObjectID AudioEventRTS::getObjectID()
{
	if (m_ownerType == OT_Object) {
		return m_objectID;
	}

	return INVALID_ID;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setDrawableID( DrawableID drawID )
{
	if (!(m_ownerType == OT_Drawable || m_ownerType == OT_INVALID)) {
		return;
	}

	m_drawableID = drawID;
	m_ownerType = OT_Drawable;
}

//-------------------------------------------------------------------------------------------------
DrawableID AudioEventRTS::getDrawableID()
{
	if (m_ownerType == OT_Drawable) {
		return m_drawableID;
	}

	return INVALID_DRAWABLE_ID;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setTimeOfDay( TimeOfDay tod )
{
	m_timeOfDay = tod;
}

//-------------------------------------------------------------------------------------------------
TimeOfDay AudioEventRTS::getTimeOfDay() const
{
	return m_timeOfDay;
}


//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setHandleToKill( AudioHandle handleToKill )
{
	m_killThisHandle = handleToKill;
}

//-------------------------------------------------------------------------------------------------
AudioHandle AudioEventRTS::getHandleToKill() const
{
	return m_killThisHandle;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setShouldFade( Bool shouldFade )
{
	m_shouldFade = shouldFade;
}

//-------------------------------------------------------------------------------------------------
Bool AudioEventRTS::getShouldFade() const
{
	return m_shouldFade;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setIsLogicalAudio( Bool isLogicalAudio )
{
	m_isLogicalAudio = isLogicalAudio;
}

//-------------------------------------------------------------------------------------------------
Bool AudioEventRTS::getIsLogicalAudio() const
{
	return m_isLogicalAudio;
}

//-------------------------------------------------------------------------------------------------
Bool AudioEventRTS::isPositionalAudio() const
{
	if( m_eventInfo )
	{
		if( !BitIsSet( m_eventInfo->m_type, ST_WORLD ) )
		{
			return FALSE;
		}
	}
	if( m_ownerType != OT_INVALID )
	{
		if( m_drawableID != INVALID_DRAWABLE_ID || m_objectID != INVALID_ID || m_ownerType == OT_Positional )
		{
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool AudioEventRTS::isCurrentlyPlaying() const
{
	return TheAudio->isCurrentlyPlaying(m_playingHandle);
}

//-------------------------------------------------------------------------------------------------
AudioPriority AudioEventRTS::getAudioPriority() const
{
	return m_priority;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setAudioPriority( AudioPriority newPriority )
{
	m_priority = newPriority;

}


//-------------------------------------------------------------------------------------------------
Real AudioEventRTS::getVolume() const
{
	if (m_volume == -1.0f) {
		if (m_eventInfo) {
			return m_eventInfo->m_volume;
		}
		return 0.5;
	}

	return m_volume;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setVolume( Real vol )
{
	m_volume = vol;
}

//-------------------------------------------------------------------------------------------------
const Coord3D *AudioEventRTS::getCurrentPosition()
{
	switch (m_ownerType)
	{
	case OT_Positional:
		return &m_positionOfAudio;

	case OT_Object:
		if (Object *obj = TheGameLogic->findObjectByID(m_objectID))
		{
			m_positionOfAudio.set( *obj->getPosition() );
		}
		else
		{
			m_ownerType = OT_Dead;
		}
		return &m_positionOfAudio;

	case OT_Drawable:
		if (Drawable *draw = TheGameClient->findDrawableByID(m_drawableID))
		{
			m_positionOfAudio.set( *draw->getPosition() );
		}
		else
		{
			m_ownerType = OT_Dead;
		}
		return &m_positionOfAudio;

	case OT_Dead:
		return &m_positionOfAudio;
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
AsciiString AudioEventRTS::generateFilenamePrefix( AudioType audioTypeToPlay, Bool localized )
{
	AsciiString retStr;
	retStr = TheAudio->getAudioSettings()->m_audioRoot;
	retStr.concat("\\");
	if (audioTypeToPlay == AT_Music) {
		retStr.concat(TheAudio->getAudioSettings()->m_musicFolder);
	} else if (audioTypeToPlay == AT_Streaming) {
		retStr.concat(TheAudio->getAudioSettings()->m_streamingFolder);
	} else {
		retStr.concat(TheAudio->getAudioSettings()->m_soundsFolder);
	}
	retStr.concat("\\");

	if (localized) {
		retStr.concat(GetRegistryLanguage());
		retStr.concat("\\");
	}

	return retStr;
}

//-------------------------------------------------------------------------------------------------
AsciiString AudioEventRTS::generateFilenameExtension( AudioType audioTypeToPlay )
{
	AsciiString retStr = AsciiString::TheEmptyString;
	if (audioTypeToPlay != AT_Music) {
		retStr = ".";
		retStr.concat(TheAudio->getAudioSettings()->m_soundsExtension);
	}

	return retStr;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::adjustForLocalization(AsciiString &strToAdjust)
{
	const char *filename = strToAdjust.reverseFind('\\');
	if (filename) {
		filename += 1;
	} else {
		filename = strToAdjust.str();
	}

	// try the localized version first so that we're guaranteed to get it
	// even if the generic data directory holds a version of the file
	AsciiString localizedFilePath = generateFilenamePrefix(m_eventInfo->m_soundType, TRUE);
	localizedFilePath.concat(filename);

	if (TheFileSystem->doesFileExist(localizedFilePath.str())) {
		strToAdjust = localizedFilePath;
	}
	// else there was no localized version, so leave the path we received unchanged
}

//-------------------------------------------------------------------------------------------------
Int AudioEventRTS::getPlayerIndex() const
{
	if (m_ownerType == OT_Object) {
		Object *obj = TheGameLogic->findObjectByID(m_objectID);
		if (obj) {
			return obj->getControllingPlayer()->getPlayerIndex();
		}
	} else if (m_ownerType == OT_Drawable) {
		Drawable *draw = TheGameClient->findDrawableByID(m_drawableID);
		if (draw) {
			Object *obj = draw->getObject();
			if (obj) {
				return obj->getControllingPlayer()->getPlayerIndex();
			}
		}
	}

	return m_playerIndex;
}

//-------------------------------------------------------------------------------------------------
void AudioEventRTS::setPlayerIndex( Int playerNdx )
{
	m_playerIndex = playerNdx;
}
