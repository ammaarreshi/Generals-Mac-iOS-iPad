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

// FILE: MilesAudioManager.cpp
/*---------------------------------------------------------------------------*/
/* EA Pacific                                                                */
/* Confidential Information	                                                 */
/* Copyright (C) 2001 - All Rights Reserved                                  */
/* DO NOT DISTRIBUTE                                                         */
/*---------------------------------------------------------------------------*/
/* Project:    RTS3                                                          */
/* File name:  MilesAudioManager.cpp                                         */
/* Created:    John K. McDonald, Jr., 3/21/2002                              */
/* Desc:       This is the implementation for the MilesAudioManager, which   */
/*						 interfaces with the Miles Sound System.                       */
/* Revision History:                                                         */
/*		7/18/2002 : Initial creation                                           */
/*---------------------------------------------------------------------------*/

// GeneralsX @build BenderAI 13/02/2026 Wrap DirectSound header (fighter19 pattern)
#ifdef _WIN32
#include <dsound.h>
#endif

#include "Lib/BaseType.h"
#include "MilesAudioDevice/MilesAudioManager.h"

#include "Common/AudioAffect.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/AudioRequest.h"
#include "Common/AudioSettings.h"
#include "Common/AsciiString.h"
#include "Common/AudioEventInfo.h"
#include "Common/FileSystem.h"
#include "Common/GameCommon.h"
#include "Common/GameSounds.h"
#include "Common/CRCDebug.h"
#include "Common/GlobalData.h"
#include "Common/ScopedMutex.h"

#include "GameClient/DebugDisplay.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/VideoPlayer.h"
#include "GameClient/View.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/TerrainLogic.h"

#include "Common/file.h"

#include <Utility/interlocked_adapter.h>


enum { INFINITE_LOOP_COUNT = 1000000 };

// Callback functions useful for Miles ////////////////////////////////////////////////////////////
static void AILCALLBACK setSampleCompleted( HSAMPLE sampleCompleted );
static void AILCALLBACK set3DSampleCompleted( H3DSAMPLE sample3DCompleted );
static void AILCALLBACK setStreamCompleted( HSTREAM streamCompleted );

static U32 AILCALLBACK streamingFileOpen(char const *fileName, void **file_handle);
static void AILCALLBACK streamingFileClose(void *fileHandle);
static S32 AILCALLBACK streamingFileSeek(void *fileHandle, S32 offset, U32 type);
static U32 AILCALLBACK streamingFileRead(void *fileHandle, void *buffer, U32 bytes);

//-------------------------------------------------------------------------------------------------
MilesAudioManager::MilesAudioManager() :
	m_providerCount(0),
	m_selectedProvider(PROVIDER_ERROR),
	m_selectedSpeakerType(0),
	m_lastProvider(PROVIDER_ERROR),
	m_digitalHandle(nullptr),
	m_num2DSamples(0),
	m_num3DSamples(0),
	m_numStreams(0),
	m_delayFilter(nullptr),
	m_binkHandle(nullptr),
	m_pref3DProvider(AsciiString::TheEmptyString),
	m_prefSpeaker(AsciiString::TheEmptyString)
{
	m_audioCache = NEW AudioFileCache;
}

//-------------------------------------------------------------------------------------------------
MilesAudioManager::~MilesAudioManager()
{
	DEBUG_ASSERTCRASH(m_binkHandle == nullptr, ("Leaked a Bink handle. Chuybregts"));
	releaseHandleForBink();
	closeDevice();
	delete m_audioCache;

	DEBUG_ASSERTCRASH(this == TheAudio, ("Umm..."));
	TheAudio = nullptr;
}

//-------------------------------------------------------------------------------------------------
#if defined(RTS_DEBUG)
AudioHandle MilesAudioManager::addAudioEvent( const AudioEventRTS *eventToAdd )
{
	if (TheGlobalData->m_preloadReport) {
		if (!eventToAdd->getEventName().isEmpty()) {
			m_allEventsLoaded.insert(eventToAdd->getEventName());
		}
	}

	return AudioManager::addAudioEvent(eventToAdd);
}
#endif

#if defined(RTS_DEBUG)
//-------------------------------------------------------------------------------------------------
void MilesAudioManager::audioDebugDisplay(DebugDisplayInterface *dd, void *, FILE *fp )
{
	std::list<PlayingAudio *>::iterator it;

	static char buffer[128] = { 0 };
	if (buffer[0] == 0) {
		AIL_MSS_version(buffer, 128);
	}

	Coord3D lookPos = TheTacticalView->getPosition();
	const Coord3D *mikePos = TheAudio->getListenerPosition();
	Coord3D distanceVector = TheTacticalView->get3DCameraPosition();
	distanceVector.sub( *mikePos );

	Int now = TheGameLogic->getFrame();
	static Int lastCheck = now;
	const Int frames = 60;
	static Int latency = 0;
	static Int worstLatency = 0;
	if( lastCheck + frames <= now )
	{
		latency = AIL_get_timer_highest_delay();
		if( latency > worstLatency )
		{
			worstLatency = latency;
		}
		lastCheck = now;
	}

	if( dd )
	{
		dd->printf("Miles Sound System version: %s    ", buffer);
		dd->printf("Memory Usage : %d/%d\n", m_audioCache->getCurrentlyUsedSize(), m_audioCache->getMaxSize());
		dd->printf("Sound: %s    ", (isOn(AudioAffect_Sound) ? "Yes" : "No"));
		dd->printf("3DSound: %s    ", (isOn(AudioAffect_Sound3D) ? "Yes" : "No"));
		dd->printf("Speech: %s    ", (isOn(AudioAffect_Speech) ? "Yes" : "No"));
		dd->printf("Music: %s\n", (isOn(AudioAffect_Music) ? "Yes" : "No"));
		dd->printf("Channels Available: ");
		dd->printf("%u Sounds    ", getNumAvailable2DSamples());
		dd->printf("%u 3D Sounds\n", getNumAvailable3DSamples());
		dd->printf("Volume: ");
		dd->printf("Sound: %d    ", REAL_TO_INT(m_soundVolume * 100.0f));
		dd->printf("3DSound: %d    ", REAL_TO_INT(m_sound3DVolume * 100.0f));
		dd->printf("Speech: %d    ", REAL_TO_INT(m_speechVolume * 100.0f));
		dd->printf("Music: %d\n", REAL_TO_INT(m_musicVolume * 100.0f));
		dd->printf("Current 3D Provider: %s    ",

		TheAudio->getProviderName(m_selectedProvider).str());
		dd->printf("Current Speaker Type: %s\n", TheAudio->translateUnsignedIntToSpeakerType(TheAudio->getSpeakerType()).str());

		dd->printf( "Looking at: (%d,%d,%d) -- Microphone at: (%d,%d,%d)\n",
				(Int)lookPos.x, (Int)lookPos.y, (Int)lookPos.z, (Int)mikePos->x, (Int)mikePos->y, (Int)mikePos->z );
		dd->printf( "Camera distance from microphone: %d -- Zoom Volume: %d%%\n",
				(Int)distanceVector.length(), (Int)(TheAudio->getZoomVolume()*100.0f) );
		dd->printf( "Worst latency: %d -- Current latency: %d\n", worstLatency, latency );

		dd->printf("-----------------------------------------------------------\n");
		dd->printf("Playing Audio\n");
	}
	if( fp )
	{
		fprintf( fp, "Miles Sound System version: %s    ", buffer );
		fprintf( fp, "Memory Usage : %d/%d\n", m_audioCache->getCurrentlyUsedSize(), m_audioCache->getMaxSize() );
		fprintf( fp, "Sound: %s    ", (isOn(AudioAffect_Sound) ? "Yes" : "No") );
		fprintf( fp, "3DSound: %s    ", (isOn(AudioAffect_Sound3D) ? "Yes" : "No") );
		fprintf( fp, "Speech: %s    ", (isOn(AudioAffect_Speech) ? "Yes" : "No") );
		fprintf( fp, "Music: %s\n", (isOn(AudioAffect_Music) ? "Yes" : "No") );
		fprintf( fp, "Channels Available: " );
		fprintf( fp, "%u Sounds    ", getNumAvailable2DSamples() );
		fprintf( fp, "%u 3D Sounds\n", getNumAvailable3DSamples() );
		fprintf( fp, "Volume: ");
		fprintf( fp, "Sound: %d    ", REAL_TO_INT(m_soundVolume * 100.0f) );
		fprintf( fp, "3DSound: %d    ", REAL_TO_INT(m_sound3DVolume * 100.0f) );
		fprintf( fp, "Speech: %d    ", REAL_TO_INT(m_speechVolume * 100.0f) );
		fprintf( fp, "Music: %d\n", REAL_TO_INT(m_musicVolume * 100.0f) );
		fprintf( fp, "Current 3D Provider: %s    ", TheAudio->getProviderName(m_selectedProvider).str());
		fprintf( fp, "Current Speaker Type: %s\n", TheAudio->translateUnsignedIntToSpeakerType(TheAudio->getSpeakerType()).str());

		fprintf( fp, "Looking at: (%d,%d,%d) -- Microphone at: (%d,%d,%d)\n",
				(Int)lookPos.x, (Int)lookPos.y, (Int)lookPos.z, (Int)mikePos->x, (Int)mikePos->y, (Int)mikePos->z );
		fprintf( fp, "Camera distance from microphone: %d -- Zoom Volume: %d%%\n",
				(Int)distanceVector.length(), (Int)(TheAudio->getZoomVolume()*100.0f) );

		fprintf( fp, "-----------------------------------------------------------\n" );
		fprintf( fp, "Playing Audio\n" );
	}

	PlayingAudio *playing = nullptr;
	Int channel;
	Int channelCount;
	Real volume = 0.0f;
	AsciiString filenameNoSlashes;

	const Int maxChannels = 64;
	PlayingAudio *playingArray[maxChannels] = { nullptr };

	// 2-D Sounds
	if( dd )
	{
		dd->printf("-----------------------------------------------------Sounds\n");
		channelCount = TheAudio->getNum2DSamples();
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			playing = *it;
			playingArray[(int)AIL_sample_user_data(playing->m_sample, 0)] = playing;
		}

		for (Int i = 1; i <= maxChannels && i <= channelCount; ++i) {
			playing = playingArray[i];
			if (!playing) {
				dd->printf("%d: Silence\n", i);
				continue;
			}

			filenameNoSlashes = playing->m_audioEventRTS->getFilename();
			filenameNoSlashes = filenameNoSlashes.reverseFind('\\') + 1;

			// Calculate Sample volume
			volume = 100.0f;
			volume *= getEffectiveVolume(playing->m_audioEventRTS);

			dd->printf("%2d: %-20s - (%s) Volume: %d (2D)\n", i, playing->m_audioEventRTS->getEventName().str(), filenameNoSlashes.str(), REAL_TO_INT(volume));
			playingArray[i] = nullptr;
		}
	}
	if( fp )
	{
		fprintf( fp, "-----------------------------------------------------Sounds\n" );
		channelCount = TheAudio->getNum2DSamples();
		channel = 1;
		for( it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it )
		{
			playing = *it;
			filenameNoSlashes = playing->m_audioEventRTS->getFilename();
			filenameNoSlashes = filenameNoSlashes.reverseFind( '\\' ) + 1;

			// Calculate Sample volume
			volume = 100.0f;
			volume *= getEffectiveVolume( playing->m_audioEventRTS );

			fprintf( fp, "%2d: %-20s - (%s) Volume: %d (2D)\n", channel++, playing->m_audioEventRTS->getEventName().str(), filenameNoSlashes.str(), REAL_TO_INT( volume ) );
		}
		for( int i = channel; i <= channelCount; ++i )
		{
			fprintf( fp, "%d: Silence\n", i );
		}
	}

	const Coord3D *microphonePos = TheAudio->getListenerPosition();

	// Now 3D Sounds
	if( dd )
	{
		dd->printf("--------------------------------------------------3D Sounds\n");
		channelCount = TheAudio->getNum3DSamples();
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			playing = *it;
			playingArray[(int)AIL_3D_user_data(playing->m_3DSample, 0)] = playing;
		}

		for (Int i = 1; i <= maxChannels && i <= channelCount; ++i)
		{
			playing = playingArray[i];
			if (!playing)
			{
				dd->printf("%d: Silence\n", i);
				continue;
			}

			filenameNoSlashes = playing->m_audioEventRTS->getFilename();
			filenameNoSlashes = filenameNoSlashes.reverseFind('\\') + 1;

			// Calculate Sample volume
			volume = 100.0f;
			volume *= getEffectiveVolume(playing->m_audioEventRTS);
			Real dist = -1.0f;
			const Coord3D *pos = playing->m_audioEventRTS->getPosition();
			char distStr[32];
			if( pos )
			{
				Coord3D vector = *microphonePos;
				vector.sub( *pos );
				dist = vector.length();
				sprintf( distStr, "%d", REAL_TO_INT( dist ) );
			}
			else
			{
				sprintf( distStr, "???" );
			}
			char str[32];
			switch( playing->m_audioEventRTS->getOwnerType() )
			{
				case OT_Positional:
					sprintf( str, "(3D)" );
					break;
				case OT_Object:
					sprintf( str, "(3DObj)" );
					break;
				case OT_Drawable:
					sprintf( str, "(3DDraw)" );
					break;
				case OT_Dead:
					sprintf( str, "(3DDead)" );
					break;

			}

			dd->printf("%2d: %-20s - (%s) Volume: %d, Dist: %s, %s\n",
				i, playing->m_audioEventRTS->getEventName().str(), filenameNoSlashes.str(), REAL_TO_INT(volume), distStr, str );
			playingArray[i] = nullptr;
		}
	}
	if( fp )
	{
		fprintf( fp, "--------------------------------------------------3D Sounds\n" );
		channelCount = TheAudio->getNum3DSamples();
		channel = 1;
		for( it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it )
		{
			playing = *it;
			filenameNoSlashes = playing->m_audioEventRTS->getFilename();
			filenameNoSlashes = filenameNoSlashes.reverseFind('\\') + 1;

			// Calculate Sample volume
			volume = 100.0f;
			volume *= getEffectiveVolume( playing->m_audioEventRTS );
			fprintf( fp, "%2d: %-24s - (%s) Volume: %d \n", channel++, playing->m_audioEventRTS->getEventName().str(), filenameNoSlashes.str(), REAL_TO_INT( volume ) );
		}

		for( int i = channel; i <= channelCount; ++i )
		{
			fprintf( fp, "%2d: Silence\n", i );
		}
	}

	// Now Streams
	if( dd )
	{
		dd->printf("----------------------------------------------------Streams\n");
		channelCount = TheAudio->getNumStreams();
		channel = 1;
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
			playing = *it;
			filenameNoSlashes = playing->m_audioEventRTS->getFilename();
			filenameNoSlashes = filenameNoSlashes.reverseFind('\\') + 1;

			// Calculate Sample volume
			volume = 100.0f;
			volume *= getEffectiveVolume(playing->m_audioEventRTS);

			dd->printf("%2d: %-24s - (%s)  Volume: %d (Stream)\n", channel++, playing->m_audioEventRTS->getEventName().str(), filenameNoSlashes.str(), REAL_TO_INT(volume));
		}

		for ( int i = channel; i <= channelCount; ++i) {
			dd->printf("%2d: Silence\n", i);
		}
		dd->printf("===========================================================\n");
	}
	if( fp )
	{
		fprintf( fp, "----------------------------------------------------Streams\n" );
		channelCount = TheAudio->getNumStreams();
		channel = 1;
		for( it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it )
		{
			playing = *it;
			filenameNoSlashes = playing->m_audioEventRTS->getFilename();
			filenameNoSlashes = filenameNoSlashes.reverseFind('\\') + 1;

			// Calculate Sample volume
			volume = 100.0f;
			volume *= getEffectiveVolume(playing->m_audioEventRTS);

			fprintf( fp, "%2d: %-24s - (%s)  Volume: %d (Stream)\n", channel++, playing->m_audioEventRTS->getEventName().str(), filenameNoSlashes.str(), REAL_TO_INT( volume ) );
		}

		for( int i = channel; i <= channelCount; ++i )
		{
			fprintf( fp, "%2d: Silence\n", i );
		}
		fprintf( fp, "===========================================================\n" );
	}
}
#endif

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::init()
{
	AudioManager::init();
#ifdef INTENSE_DEBUG
	DEBUG_LOG(("Sound has temporarily been disabled in debug builds only. jkmcd"));
	// for now, RTS_DEBUG builds only should have no sound. ask jkmcd or srj about this.
	return;
#endif

	// We should now know how many samples we want to load
	openDevice();
	m_audioCache->setMaxSize(getAudioSettings()->m_maxCacheSize);

	// Now, set the file callbacks to load the streams from Biggie files
	AIL_set_file_callbacks(streamingFileOpen, streamingFileClose, streamingFileSeek, streamingFileRead);
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::postProcessLoad()
{
	AudioManager::postProcessLoad();
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::reset()
{
#if defined(RTS_DEBUG)
	dumpAllAssetsUsed();
	m_allEventsLoaded.clear();
#endif

	AudioManager::reset();
	stopAllAudioImmediately();
  removeAllAudioRequests();
  // This must come after stopAllAudioImmediately() and removeAllAudioRequests(), to ensure that
  // sounds pointing to the temporary AudioEventInfo handles are deleted before their info is deleted
  removeLevelSpecificAudioEventInfos();
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::update()
{
	AudioManager::update();
	setDeviceListenerPosition();
	processRequestList();
	processPlayingList();
	processFadingList();
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::stopAudio( AudioAffect which )
{
	// All we really need to do is:
	// 1) Remove the EOS callback.
	// 2) Stop the sample, (so that when we later unload it, bad stuff doesn't happen)
	// 3) Set the status to stopped, so that when we next process the playing list, we will
	//		correctly clean up the sample.

	std::list<PlayingAudio *>::iterator it;

	PlayingAudio *playing = nullptr;
	if (BitIsSet(which, AudioAffect_Sound)) {
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			playing = *it;
			stopPlayingAudio(playing);
		}
	}

	if (BitIsSet(which, AudioAffect_Sound3D)) {
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			playing = *it;
			stopPlayingAudio(playing);
		}
	}

	if (BitIsSet(which, AudioAffect_Speech | AudioAffect_Music)) {
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
			playing = *it;
			if (playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music) {
				if (!BitIsSet(which, AudioAffect_Music)) {
					continue;
				}
			} else {
				if (!BitIsSet(which, AudioAffect_Speech)) {
					continue;
				}
			}
			stopPlayingAudio(playing);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::pauseAudio( AudioAffect which )
{
	std::list<PlayingAudio *>::iterator it;

	PlayingAudio *playing = nullptr;
	if (BitIsSet(which, AudioAffect_Sound)) {
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			playing = *it;
			DEBUG_ASSERTCRASH(playing->m_sample, ("Sample is not expected to be null"));
			AIL_stop_sample(playing->m_sample);
		}
	}

	if (BitIsSet(which, AudioAffect_Sound3D)) {
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			playing = *it;
			DEBUG_ASSERTCRASH(playing->m_sample, ("3D Sample is not expected to be null"));
			AIL_stop_3D_sample(playing->m_3DSample);
		}
	}

	if (BitIsSet(which, AudioAffect_Speech | AudioAffect_Music)) {
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
			playing = *it;
			if (playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music) {
				if (!BitIsSet(which, AudioAffect_Music)) {
					continue;
				}
			} else {
				if (!BitIsSet(which, AudioAffect_Speech)) {
					continue;
				}
			}
			DEBUG_ASSERTCRASH(playing->m_sample, ("Stream is not expected to be null"));
			AIL_pause_stream(playing->m_stream, 1);
		}
	}

	//Get rid of PLAY audio requests when pausing audio.
	std::list<AudioRequest*>::iterator ait;
	for (ait = m_audioRequests.begin(); ait != m_audioRequests.end(); )
	{
		AudioRequest *req = (*ait);
		if( req->m_request == AR_Play )
		{
			deleteInstance(req);
			ait = m_audioRequests.erase(ait);
		}
		else
		{
			ait++;
		}
	}
}


//-------------------------------------------------------------------------------------------------
void MilesAudioManager::resumeAudio( AudioAffect which )
{
	std::list<PlayingAudio *>::iterator it;

	PlayingAudio *playing = nullptr;
	if (BitIsSet(which, AudioAffect_Sound)) {
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			playing = *it;
			DEBUG_ASSERTCRASH(playing->m_sample, ("Sample is not expected to be null"));
			AIL_resume_sample(playing->m_sample);
		}
	}

	if (BitIsSet(which, AudioAffect_Sound3D)) {
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			playing = *it;
			DEBUG_ASSERTCRASH(playing->m_3DSample, ("3D Sample is not expected to be null"));
			AIL_resume_3D_sample(playing->m_3DSample);
		}
	}

	if (BitIsSet(which, AudioAffect_Speech | AudioAffect_Music)) {
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
			playing = *it;
			if (playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music) {
				if (!BitIsSet(which, AudioAffect_Music)) {
					continue;
				}
			} else {
				if (!BitIsSet(which, AudioAffect_Speech)) {
					continue;
				}
			}
			DEBUG_ASSERTCRASH(playing->m_stream, ("Stream is not expected to be null"));
			AIL_pause_stream(playing->m_stream, 0);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::pauseAmbient( Bool shouldPause )
{

}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::playAudioEvent( AudioEventRTS *event )
{
#ifdef INTENSIVE_AUDIO_DEBUG
	DEBUG_LOG(("MILES (%d) - Processing play request: %d (%s)", TheGameLogic->getFrame(), event->getPlayingHandle(), event->getEventName().str()));
#endif
	const AudioEventInfo *info = event->getAudioEventInfo();
	if (!info) {
		return;
	}

	std::list<PlayingAudio *>::iterator it;
	PlayingAudio *playing = nullptr;

	AudioHandle handleToKill = event->getHandleToKill();

	AsciiString fileToPlay = event->getFilename();
	PlayingAudio *audio = allocatePlayingAudio();
	switch(info->m_soundType)
	{
		case AT_Music:
		case AT_Streaming:
		{
		#ifdef INTENSIVE_AUDIO_DEBUG
			DEBUG_LOG(("- Stream"));
		#endif

			if ((info->m_soundType == AT_Streaming) && event->getUninterruptible()) {
				stopAllSpeech();
			}

			Bool foundSoundToReplace = false;
			if (handleToKill) {
				for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
					playing = (*it);

					if (playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == handleToKill)
					{
						//Release this streaming channel immediately because we are going to play another sound in it's place.
						stopPlayingAudio(playing);
						foundSoundToReplace = true;
						break;
					}
				}
			}

			HSTREAM stream;
			if (!handleToKill || foundSoundToReplace) {
				stream = AIL_open_stream(m_digitalHandle, fileToPlay.str(), 0);
			} else {
				stream = nullptr;
			}

			// Put this on here, so that the audio event RTS will be cleaned up regardless.
			audio->m_audioEventRTS = event;
			audio->m_stream = stream;
			audio->m_type = PAT_Stream;

			if (stream) {
				if ((info->m_soundType == AT_Streaming) && event->getUninterruptible()) {
					setDisallowSpeech(TRUE);
	 			}
				AIL_set_stream_volume_pan(stream, getEffectiveVolume(event), 0.5f);
				playStream(event, stream);

				CriticalSectionClass::LockClass lock(m_playingStreamsCS);
				m_playingStreams.push_back(audio);
				audio = nullptr;
			}
			break;
		}

		case AT_SoundEffect:
		{
		#ifdef INTENSIVE_AUDIO_DEBUG
			DEBUG_LOG(("- Sound"));
		#endif


			if (event->isPositionalAudio()) {
				// Sounds that are non-global are positional 3-D sounds. Deal with them accordingly
			#ifdef INTENSIVE_AUDIO_DEBUG
				DEBUG_LOG((" Positional"));
			#endif
				Bool foundSoundToReplace = false;
				if (handleToKill)
				{
					for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
						playing = (*it);

						if( playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == handleToKill )
						{
							//Release this 3D sound channel immediately because we are going to play another sound in it's place.
							stopPlayingAudio(playing);
							foundSoundToReplace = true;
							break;
						}
					}
				}

				H3DSAMPLE sample3D;
				if( !handleToKill || foundSoundToReplace )
				{
					sample3D = getAvailable3DSample( event );
					if( !sample3D )
					{
						//If we don't have an available sample, kill the lowest priority assuming we have one that is lower
						//than the sound we are trying to add. One possibility for strangeness is when an interrupt sound
						//that wants to kill a handle to replace it, it's possible that another request already killed it,
						//in which case we need to attempt to find another sound to kill.
						if( killLowestPrioritySoundImmediately( event ) )
						{
							sample3D = getAvailable3DSample( event );
						}
					}
				}
				else
				{
					sample3D = nullptr;
				}
				// Push it onto the list of playing things
				audio->m_audioEventRTS = event;
				audio->m_3DSample = sample3D;
				audio->m_file = nullptr;
				audio->m_type = PAT_3DSample;

				if (sample3D) {
					audio->m_file = playSample3D(event, sample3D);
				}

				if( !audio->m_file )
				{
					#ifdef INTENSIVE_AUDIO_DEBUG
						DEBUG_LOG((" Killed (no handles available)"));
					#endif
				}
				else
				{
					CriticalSectionClass::LockClass lock(m_playing3DSoundsCS);
					m_playing3DSounds.push_back(audio);
					audio = nullptr;
					#ifdef INTENSIVE_AUDIO_DEBUG
						DEBUG_LOG((" Playing."));
					#endif
				}
			}
			else
			{
				// UI sounds are always 2-D. All other sounds should be Positional
				// Unit acknowledgment, etc, falls into the UI category of sound.
				Bool foundSoundToReplace = false;
				if (handleToKill) {
					for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
						playing = (*it);

						if (playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == handleToKill)
						{
							//Release this 2D sound channel immediately because we are going to play another sound in it's place.
							stopPlayingAudio(playing);
							foundSoundToReplace = true;
							break;
						}
					}
				}

				HSAMPLE sample;
				if( !handleToKill || foundSoundToReplace )
				{
					sample = getAvailable2DSample(event);
					if( !sample )
					{
						//If we don't have an available sample, kill the lowest priority assuming we have one that is lower
						//than the sound we are trying to add. One possibility for strangeness is when an interrupt sound
						//that wants to kill a handle to replace it, it's possible that another request already killed it,
						//in which case we need to attempt to find another sound to kill.
						if( killLowestPrioritySoundImmediately( event ) )
						{
							sample = getAvailable2DSample( event );
						}
					}
					}
				else
				{
					sample = nullptr;
				}

				// Push it onto the list of playing things
				audio->m_audioEventRTS = event;
				audio->m_sample = sample;
				audio->m_file = nullptr;
				audio->m_type = PAT_Sample;

				if (sample) {
					audio->m_file = playSample(event, sample);
				}

				if (!audio->m_file) {
					#ifdef INTENSIVE_AUDIO_DEBUG
						DEBUG_LOG((" Killed (no handles available)"));
					#endif
				} else {
					CriticalSectionClass::LockClass lock(m_playingSoundsCS);
					m_playingSounds.push_back(audio);
					audio = nullptr;
					#ifdef INTENSIVE_AUDIO_DEBUG
						DEBUG_LOG((" Playing."));
					#endif
				}
			}
			break;
		}
	}

	// If we were able to successfully play audio, then we set it to null above. (And it will be freed
	// later. However, if audio is non-null at this point, then it must be freed.
	if (audio) {
		stopPlayingAudio(audio);
		releasePlayingAudio(audio);
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::stopAudioEvent( AudioHandle handle )
{
#ifdef INTENSIVE_AUDIO_DEBUG
	DEBUG_LOG(("MILES (%d) - Processing stop request: %d", TheGameLogic->getFrame(), handle));
#endif

	std::list<PlayingAudio *>::iterator it;
	if ( handle == AHSV_StopTheMusic || handle == AHSV_StopTheMusicFade ) {
		// for music, find and stop the currently playing music stream(s).
		for ( it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it ) {
			PlayingAudio *audio = (*it);

			if( audio->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music )
			{
				if( handle == AHSV_StopTheMusicFade )
				{
					fadePlayingAudio(audio);
				}
				else
				{
					stopPlayingAudio(audio);
				}
			}
		}
		return;
	}

	for ( it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it ) {
		PlayingAudio *audio = (*it);

		if (audio->m_audioEventRTS->getPlayingHandle() == handle) {
			stopPlayingAudio(audio);
			break;
		}
	}

	for ( it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it ) {
		PlayingAudio *audio = (*it);

		if (audio->m_audioEventRTS->getPlayingHandle() == handle) {
			stopPlayingAudio(audio);
			break;
		}
	}

	for ( it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it ) {
		PlayingAudio *audio = (*it);

		if (audio->m_audioEventRTS->getPlayingHandle() == handle) {
		#ifdef INTENSIVE_AUDIO_DEBUG
			DEBUG_LOG((" (%s)", audio->m_audioEventRTS->getEventName()));
		#endif
			stopPlayingAudio(audio);
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::killAudioEventImmediately( AudioHandle audioEvent )
{
	//First look for it in the request list.
	std::list<AudioRequest*>::iterator ait;
	for( ait = m_audioRequests.begin(); ait != m_audioRequests.end(); ait++ )
	{
		AudioRequest *req = (*ait);
		if( req->m_usePendingEvent && req->m_pendingEvent->getPlayingHandle() == audioEvent )
		{
			deleteInstance(req);
			ait = m_audioRequests.erase(ait);
			return;
		}
	}

	//Look for matching 3D sound to kill
	std::list<PlayingAudio *>::iterator it;
	for( it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); it++ )
	{
		PlayingAudio *audio = (*it);

		if( audio->m_audioEventRTS->getPlayingHandle() == audioEvent )
		{
			stopPlayingAudio(audio);
			return;
		}
	}

	//Look for matching 2D sound to kill
	for( it = m_playingSounds.begin(); it != m_playingSounds.end(); it++ )
	{
		PlayingAudio *audio = (*it);

		if( audio->m_audioEventRTS->getPlayingHandle() == audioEvent )
		{
			stopPlayingAudio(audio);
			return;
		}
	}

	//Look for matching steaming sound to kill
	for( it = m_playingStreams.begin(); it != m_playingStreams.end(); it++ )
	{
		PlayingAudio *audio = (*it);

		if( audio->m_audioEventRTS->getPlayingHandle() == audioEvent )
		{
			stopPlayingAudio(audio);
			return;
		}
	}

}


//-------------------------------------------------------------------------------------------------
void MilesAudioManager::pauseAudioEvent( AudioHandle handle )
{
	// pause audio
}

//-------------------------------------------------------------------------------------------------
void *MilesAudioManager::loadFileForRead( AudioEventRTS *eventToLoadFrom )
{
	return m_audioCache->openFile(eventToLoadFrom);
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::closeFile( void *fileRead )
{
	m_audioCache->closeFile(fileRead);
}


//-------------------------------------------------------------------------------------------------
PlayingAudio *MilesAudioManager::allocatePlayingAudio()
{
	return NEW PlayingAudio; // poolify
}


//-------------------------------------------------------------------------------------------------
void MilesAudioManager::releaseMilesHandles( PlayingAudio *release )
{
	switch (release->m_type)
	{
		case PAT_Sample:
		{
			if (release->m_sample) {
				AIL_register_EOS_callback(release->m_sample, nullptr);
				AIL_stop_sample(release->m_sample);
				m_availableSamples.push_back(release->m_sample);
				release->m_sample = nullptr;
			}
			break;
		}
		case PAT_3DSample:
		{
			if (release->m_3DSample) {
				AIL_register_3D_EOS_callback(release->m_3DSample, nullptr);
				AIL_stop_3D_sample(release->m_3DSample);
				m_available3DSamples.push_back(release->m_3DSample);
				release->m_3DSample = nullptr;
			}
			break;
		}
		case PAT_Stream:
		{
			if (release->m_stream) {
				AIL_register_stream_callback(release->m_stream, nullptr);
				AIL_close_stream(release->m_stream);
				release->m_stream = nullptr;
			}
			break;
		}
	}
	release->m_type = PAT_INVALID;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::releasePlayingAudio( PlayingAudio *release )
{
	DEBUG_ASSERTCRASH(release->m_status == PS_Stopped, ("PlayingAudio must be stopped by now"));
	DEBUG_ASSERTCRASH(release->m_type == PAT_INVALID, ("PlayingAudio must be invalidated by now"));

	if (release->m_file) {
		closeFile(release->m_file);
		release->m_file = nullptr;
	}

	if (release->m_cleanupAudioEventRTS) {
		releaseAudioEventRTS(release->m_audioEventRTS);
	}

	delete release;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::stopPlayingAudio( PlayingAudio *release )
{
	// Advance from Playing to Stopping
	InterlockedCompareExchange(reinterpret_cast<volatile long*>(&release->m_status), PS_Stopping, PS_Playing);
	// Advance from Stopping to Stopped
	const long prevStatus = InterlockedCompareExchange(reinterpret_cast<volatile long*>(&release->m_status), PS_Stopped, PS_Stopping);
	if (prevStatus != PS_Stopping) {
		return;
	}
	releaseMilesHandles(release);
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::fadePlayingAudio( PlayingAudio *playing )
{
	playing->m_fade = true;
}

//-------------------------------------------------------------------------------------------------
PlayingAudio *MilesAudioManager::findActiveMusic( const AsciiString* trackName )
{
	std::list<PlayingAudio *>::const_iterator it;
	PlayingAudio *playing;
	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = *it;
		if (playing->m_status != PS_Playing) {
			continue;
		}
		if (playing->m_fade) {
			continue;
		}
		if (playing->m_audioEventRTS->getAudioEventInfo()->m_soundType != AT_Music) {
			continue;
		}
		if (trackName && *trackName != playing->m_audioEventRTS->getEventName()) {
			continue;
		}
		return playing;
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
const PlayingAudio *MilesAudioManager::findActiveMusic( const AsciiString* trackName ) const
{
	return const_cast<MilesAudioManager*>(this)->findActiveMusic(trackName);
}


//-------------------------------------------------------------------------------------------------
void MilesAudioManager::releasePlayingAudioInListIfStopped(std::list<PlayingAudio *> &list, CriticalSectionClass &cs)
{
	std::list<PlayingAudio *>::iterator it;
	PlayingAudio *playing;
	CriticalSectionClass::LockClass lock(cs);

	for (it = list.begin(); it != list.end(); )
	{
		playing = (*it);

		if (playing->m_status == PS_Stopped)
		{
			releasePlayingAudio(playing);
			it = list.erase(it);
			continue;
		}

		++it;
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::stopAllAudioImmediately()
{
	std::list<PlayingAudio *>::iterator it;
	PlayingAudio *playing;

	//
	// Stop audio without locking. Calls into Miles.
	//
	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		playing = (*it);
		stopPlayingAudio(playing);
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
		playing = (*it);
		stopPlayingAudio(playing);
	}

	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = (*it);
		stopPlayingAudio(playing);
	}

	for (it = m_fadingAudio.begin(); it != m_fadingAudio.end(); ++it) {
		playing = (*it);
		stopPlayingAudio(playing);
	}

	//
	// Release audio with locking. Must not call into Miles!
	//
	releasePlayingAudioInListIfStopped(m_playingSounds, m_playingSoundsCS);
	releasePlayingAudioInListIfStopped(m_playing3DSounds, m_playing3DSoundsCS);
	releasePlayingAudioInListIfStopped(m_playingStreams, m_playingStreamsCS);
	releasePlayingAudioInListIfStopped(m_fadingAudio, m_fadingAudioCS);

	DEBUG_ASSERTCRASH(m_playingSounds.empty(), ("List is expected to be empty now"));
	DEBUG_ASSERTCRASH(m_playing3DSounds.empty(), ("List is expected to be empty now"));
	DEBUG_ASSERTCRASH(m_playingStreams.empty(), ("List is expected to be empty now"));
	DEBUG_ASSERTCRASH(m_fadingAudio.empty(), ("List is expected to be empty now"));

	std::list<HAUDIO>::iterator hit;
	for (hit = m_audioForcePlayed.begin(); hit != m_audioForcePlayed.end(); ++hit) {
		if (*hit) {
			AIL_quick_unload(*hit);
		}
	}

	m_audioForcePlayed.clear();
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::freeAllMilesHandles()
{
	// First, we need to ensure that we don't have any sample handles open. To that end, we must stop
	// all of our currently playing audio.
	stopAllAudioImmediately();

	// Walks through the available 2-D and 3-D handles and releases them
	std::vector<HSAMPLE>::iterator it;
	for ( it = m_availableSamples.begin(); it != m_availableSamples.end(); ++it ) {
		AIL_release_sample_handle(*it);
	}

	m_availableSamples.clear();
	m_num2DSamples = 0;

	std::vector<H3DSAMPLE>::iterator it3D;
	for ( it3D = m_available3DSamples.begin(); it3D != m_available3DSamples.end(); ++it3D ) {
		AIL_release_3D_sample_handle(*it3D);
	}

	m_available3DSamples.clear();
	m_num3DSamples = 0;
	m_numStreams = 0;
}

//-------------------------------------------------------------------------------------------------
HSAMPLE MilesAudioManager::getAvailable2DSample( AudioEventRTS *event )
{
	if (!m_availableSamples.empty()) {
		HSAMPLE retSample = m_availableSamples.back();
		m_availableSamples.pop_back();
		return retSample;
	}

	// Find the first sample of lower priority than my augmented priority that is interruptible and take its handle
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
H3DSAMPLE MilesAudioManager::getAvailable3DSample( AudioEventRTS *event )
{
	if (!m_available3DSamples.empty()) {
		H3DSAMPLE retSample = m_available3DSamples.back();
		m_available3DSamples.pop_back();
		return retSample;
	}

	// Find the first sample of lower priority than my augmented priority that is interruptible and take its handle
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::adjustPlayingVolume( PlayingAudio *audio )
{
	Real pan;
	if (audio->m_type == PAT_Sample) {
		AIL_sample_volume_pan(audio->m_sample, nullptr, &pan);
		AIL_set_sample_volume_pan(audio->m_sample, getEffectiveVolume(audio->m_audioEventRTS), pan);

	} else if (audio->m_type == PAT_3DSample) {
		AIL_set_3D_sample_volume(audio->m_3DSample, getEffectiveVolume(audio->m_audioEventRTS));

	} else if (audio->m_type == PAT_Stream) {
		AIL_stream_volume_pan(audio->m_stream, nullptr, &pan);
		if (audio->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music ) {
			AIL_set_stream_volume_pan(audio->m_stream, getEffectiveVolume(audio->m_audioEventRTS), pan);

		} else {
			AIL_set_stream_volume_pan(audio->m_stream, getEffectiveVolume(audio->m_audioEventRTS), pan);

		}
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::stopAllSpeech()
{
	std::list<PlayingAudio *>::iterator it;
	PlayingAudio *playing;
	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = (*it);
		if (playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Streaming) {
			stopPlayingAudio(playing);
		}
	}

}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::initFilters( HSAMPLE sample, AudioEventRTS *event )
{
	// set the sample volume
	AIL_set_sample_volume_pan(sample, getEffectiveVolume(event), 0.5f);

	// pitch shift
	Real pitchShift = event->getPitchShift();
	if (pitchShift == 0.0f) {
		DEBUG_CRASH(("Invalid Pitch shift in sound: '%s'", event->getEventName().str()) );
	} else {
		AIL_set_sample_playback_rate(sample, REAL_TO_INT(AIL_sample_playback_rate(sample) * pitchShift));
	}

	// set up delay filter, if applicable
	if (event->getDelay() > 0.0f) {
		Real value;
		value = event->getDelay();
		AIL_set_sample_processor(sample, DP_FILTER, m_delayFilter);
		AIL_set_filter_sample_preference(sample, "Mono Delay Time", &value);

		value = 0.0;
		AIL_set_filter_sample_preference(sample, "Mono Delay", &value);
		AIL_set_filter_sample_preference(sample, "Mono Delay Mix", &value);
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::initFilters3D( H3DSAMPLE sample, AudioEventRTS *event, const Coord3D *pos )
{
	// set the sample volume
	AIL_set_3D_sample_volume(sample, getEffectiveVolume(event));

	// pitch shift
	Real pitchShift = event->getPitchShift();
	if (pitchShift == 0.0f) {
		DEBUG_CRASH(("Invalid Pitch shift in sound: '%s'", event->getEventName().str()) );
	} else {
		AIL_set_3D_sample_playback_rate(sample, REAL_TO_INT(AIL_3D_sample_playback_rate(sample) * pitchShift));
	}

	// Low pass filter
	if (event->getAudioEventInfo()->m_lowPassFreq > 0 && !isOnScreen(pos) ) {
		AIL_set_3D_sample_occlusion(sample, 1.0f - event->getAudioEventInfo()->m_lowPassFreq);
	}
}

//-------------------------------------------------------------------------------------------------
AsciiString MilesAudioManager::nextMusicTrack()
{
	AsciiString trackName;

	if (PlayingAudio *playing = findActiveMusic()) {
		trackName = playing->m_audioEventRTS->getEventName();
	}

	// Stop currently playing music
	TheAudio->removeAudioEvent(AHSV_StopTheMusic);

	trackName = nextTrackName(trackName);
	AudioEventRTS newTrack(trackName);
	TheAudio->addAudioEvent(&newTrack);

	return trackName;
}

//-------------------------------------------------------------------------------------------------
AsciiString MilesAudioManager::prevMusicTrack()
{
	AsciiString trackName;

	if (PlayingAudio *playing = findActiveMusic()) {
		trackName = playing->m_audioEventRTS->getEventName();
	}

	// Stop currently playing music
	TheAudio->removeAudioEvent(AHSV_StopTheMusic);

	trackName = prevTrackName(trackName);
	AudioEventRTS newTrack(trackName);
	TheAudio->addAudioEvent(&newTrack);

	return trackName;
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::isMusicPlaying() const
{
	return findActiveMusic() != nullptr;
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::hasMusicTrackCompleted( const AsciiString& trackName, Int numberOfTimes ) const
{
	if (const PlayingAudio *playing = findActiveMusic(&trackName)) {
		if (INFINITE_LOOP_COUNT - AIL_stream_loop_count(playing->m_stream) >= numberOfTimes) {
			return TRUE;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::openDevice()
{
	if (!TheGlobalData->m_audioOn) {
		return;
	}

	AIL_set_redist_directory("MSS\\");
	AIL_startup();
	Int retval = 0;

	// AIL_quick_startup should be replaced later with a call to actually pick which device to use, etc
	const AudioSettings *audioSettings = getAudioSettings();
	m_selectedSpeakerType = TheAudio->translateSpeakerTypeToUnsignedInt(m_prefSpeaker);

	retval = AIL_quick_startup(audioSettings->m_useDigital, audioSettings->m_useMidi, audioSettings->m_outputRate, audioSettings->m_outputBits, audioSettings->m_outputChannels);

	// Quick handles tells us where to store the various devices. For now, we're only interested in the digital handle.
	AIL_quick_handles(&m_digitalHandle, nullptr, nullptr);

	if (retval) {
		buildProviderList();
	} else {
		// if we couldn't initialize any devices, turn sound off (fail silently)
		setOn( false, AudioAffect_All );
	}

	selectProvider(TheAudio->getProviderIndex(m_pref3DProvider));

	// Now that we're all done, update the cached variables so that everything is in sync.
	TheAudio->refreshCachedVariables();

	if (!isValidProvider()) {
		return;
	}

	initDelayFilter();
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::closeDevice()
{
	freeAllMilesHandles();
	unselectProvider();
	AIL_shutdown();
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::isCurrentlyPlaying( AudioHandle handle )
{
	std::list<PlayingAudio *>::iterator it;
	PlayingAudio *playing;

	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		playing = *it;
		if (playing->m_audioEventRTS->getPlayingHandle() == handle) {
			return true;
		}
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
		playing = *it;
		if (playing->m_audioEventRTS->getPlayingHandle() == handle) {
			return true;
		}
	}

	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = *it;
		if (playing->m_audioEventRTS->getPlayingHandle() == handle) {
			return true;
		}
	}

	// if something is requested, it is also considered playing
	std::list<AudioRequest *>::iterator ait;
	AudioRequest *req = nullptr;
	for (ait = m_audioRequests.begin(); ait != m_audioRequests.end(); ++ait) {
		req = *ait;
		if (req->m_usePendingEvent && req->m_pendingEvent->getPlayingHandle() == handle) {
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::notifyOfAudioCompletion( UnsignedInt handle, UnsignedInt flags )
{
	PlayingAudio *playing = findPlayingAudioFrom(handle, flags);
	if (!playing) {
		DEBUG_CRASH(("Audio has completed playing, but we can't seem to find it. - jkmcd"));
		return;
	}

	if (getDisallowSpeech() && playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Streaming) {
		setDisallowSpeech(FALSE);
	}

	if (playing->m_audioEventRTS->getAudioEventInfo()->m_control & AC_LOOP) {
		if (playing->m_audioEventRTS->getNextPlayPortion() == PP_Attack) {
			playing->m_audioEventRTS->setNextPlayPortion(PP_Sound);
		}
		if (playing->m_audioEventRTS->getNextPlayPortion() == PP_Sound) {
			// First, decrease the loop count.
			playing->m_audioEventRTS->decreaseLoopCount();

			// Now, try to start the next loop
			if (startNextLoop(playing)) {
				return;
			}
		}
	}

	playing->m_audioEventRTS->advanceNextPlayPortion();
	if (playing->m_audioEventRTS->getNextPlayPortion() != PP_Done) {
		if (playing->m_type == PAT_Sample) {
			closeFile(playing->m_file);	// close it so as not to leak it.
			playing->m_file = playSample(playing->m_audioEventRTS, playing->m_sample);

			// If we don't have a file now, then we should drop to the stopped status so that
			// We correctly close this handle.
			if (playing->m_file) {
				return;
			}
		} else if (playing->m_type == PAT_3DSample) {
			closeFile(playing->m_file);	// close it so as not to leak it.
			playing->m_file = playSample3D(playing->m_audioEventRTS, playing->m_3DSample);

			// If we don't have a file now, then we should drop to the stopped status so that
			// We correctly close this handle.
			if (playing->m_file) {
				return;
			}
		}
	}

	if (playing->m_type == PAT_Stream) {
		if (playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music) {
			playStream(playing->m_audioEventRTS, playing->m_stream);
			return;
		}
	}

	// it will be cleaned up on the next frame update
	InterlockedCompareExchange(reinterpret_cast<volatile long*>(&playing->m_status), PS_Stopping, PS_Playing);
}

//-------------------------------------------------------------------------------------------------
PlayingAudio *MilesAudioManager::findPlayingAudioFrom( UnsignedInt handle, UnsignedInt flags )
{
	std::list<PlayingAudio *>::iterator it;
	PlayingAudio *playing;

	if (flags == PAT_Sample) {
		HSAMPLE sample = (HSAMPLE) handle;
		CriticalSectionClass::LockClass lock(m_playingSoundsCS);
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			playing = *it;
			if (playing->m_sample == sample) {
				return playing;
			}
		}
	}

	if (flags == PAT_3DSample) {
		H3DSAMPLE sample3D = (H3DSAMPLE) handle;
		CriticalSectionClass::LockClass lock(m_playing3DSoundsCS);
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			playing = *it;
			if (playing->m_3DSample == sample3D) {
				return playing;
			}
		}
	}

	if (flags == PAT_Stream) {
		HSTREAM stream = (HSTREAM) handle;
		{
			CriticalSectionClass::LockClass lock(m_playingStreamsCS);
			for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
				playing = *it;
				if (playing->m_stream == stream) {
					return playing;
				}
			}
		}
		{
			CriticalSectionClass::LockClass lock(m_fadingAudioCS);
			for (it = m_fadingAudio.begin(); it != m_fadingAudio.end(); ++it) {
				playing = *it;
				if (playing->m_stream == stream) {
					return playing;
				}
			}
		}
	}

	return nullptr;
}


//-------------------------------------------------------------------------------------------------
UnsignedInt MilesAudioManager::getProviderCount() const
{
	return m_providerCount;
}

//-------------------------------------------------------------------------------------------------
AsciiString MilesAudioManager::getProviderName( UnsignedInt providerNum ) const
{
	if (isOn(AudioAffect_Sound3D) && providerNum < m_providerCount) {
		return m_provider3D[providerNum].name;
	}

	return AsciiString::TheEmptyString;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MilesAudioManager::getProviderIndex( AsciiString providerName ) const
{
	for (UnsignedInt i = 0; i < m_providerCount; ++i) {
		if (providerName == m_provider3D[i].name) {
			return i;
		}
	}

	return PROVIDER_ERROR;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::selectProvider( UnsignedInt providerNdx )
{
	if (!isOn(AudioAffect_Sound3D))
	{
		return;
	}

	if (providerNdx == m_selectedProvider)
	{
		return;
	}

	if (isValidProvider())
	{
		freeAllMilesHandles();
		unselectProvider();
	}

	LPDIRECTSOUND lpDirectSoundInfo;
	AIL_get_DirectSound_info( nullptr, (void**)&lpDirectSoundInfo, nullptr );
	Bool useDolby = FALSE;
	if( lpDirectSoundInfo )
	{
		DWORD speakerConfig;
		lpDirectSoundInfo->GetSpeakerConfig( &speakerConfig );
		switch( DSSPEAKER_CONFIG( speakerConfig ) )
		{
			case DSSPEAKER_DIRECTOUT:
				m_selectedSpeakerType = AIL_3D_2_SPEAKER;
				break;
			case DSSPEAKER_MONO:
				m_selectedSpeakerType = AIL_3D_2_SPEAKER;
				break;
			case DSSPEAKER_STEREO:
				m_selectedSpeakerType = AIL_3D_2_SPEAKER;
				break;
			case DSSPEAKER_HEADPHONE:
				m_selectedSpeakerType = AIL_3D_HEADPHONE;
				useDolby = TRUE;
				break;
			case DSSPEAKER_QUAD:
				m_selectedSpeakerType = AIL_3D_4_SPEAKER;
				useDolby = TRUE;
				break;
			case DSSPEAKER_SURROUND:
				m_selectedSpeakerType = AIL_3D_SURROUND;
				useDolby = TRUE;
				break;
			case DSSPEAKER_5POINT1:
				m_selectedSpeakerType = AIL_3D_51_SPEAKER;
				useDolby = TRUE;
				break;
			case DSSPEAKER_7POINT1:
				m_selectedSpeakerType = AIL_3D_71_SPEAKER;
				useDolby = TRUE;
				break;
		}
	}

	Bool success = FALSE;
	if( useDolby )
	{
		providerNdx = getProviderIndex( "Dolby Surround" );
	}
	else
	{
		providerNdx = getProviderIndex( "Miles Fast 2D Positional Audio" );
	}
	success = AIL_open_3D_provider( m_provider3D[providerNdx].id ) == 0;

	//if (providerNdx < m_providerCount)
	//{
	//	failed = AIL_open_3D_provider(m_provider3D[providerNdx].id);
	//}



	if( !success )
	{
		m_selectedProvider = PROVIDER_ERROR;
		// try to select a failsafe
		providerNdx = getProviderIndex( "Miles Fast 2D Positional Audio" );
		success = AIL_open_3D_provider( m_provider3D[providerNdx].id ) == 0;
	}

	if ( success )
	{
		m_selectedProvider = providerNdx;

		initSamplePools();

		createListener();
		setSpeakerType(m_selectedSpeakerType);
		if (TheVideoPlayer)
		{
			TheVideoPlayer->notifyVideoPlayerOfNewProvider(TRUE);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::unselectProvider()
{
	if (!(isOn(AudioAffect_Sound3D) && isValidProvider())) {
		return;
	}

	if (TheVideoPlayer) {
		TheVideoPlayer->notifyVideoPlayerOfNewProvider(FALSE);
	}
	AIL_close_3D_listener(m_listener);
	m_listener = nullptr;

	AIL_close_3D_provider(m_provider3D[m_selectedProvider].id);
	m_lastProvider = m_selectedProvider;

	m_selectedProvider = PROVIDER_ERROR;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MilesAudioManager::getSelectedProvider() const
{
	return m_selectedProvider;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::setSpeakerType( UnsignedInt speakerType )
{
	if (!isValidProvider()) {
		return;
	}

	AIL_set_3D_speaker_type( m_provider3D[m_selectedProvider].id, speakerType );
	m_selectedSpeakerType = speakerType;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MilesAudioManager::getSpeakerType()
{
	if (!isValidProvider()) {
		return 0;
	}

	return m_selectedSpeakerType;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MilesAudioManager::getNum2DSamples() const
{
	return m_num2DSamples;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MilesAudioManager::getNum3DSamples() const
{
	return m_num3DSamples;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MilesAudioManager::getNumStreams() const
{
	return m_numStreams;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MilesAudioManager::getNumAvailable2DSamples() const
{
	return (UnsignedInt)m_availableSamples.size();
}

//-------------------------------------------------------------------------------------------------
UnsignedInt MilesAudioManager::getNumAvailable3DSamples() const
{
	return (UnsignedInt)m_available3DSamples.size();
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::doesViolateLimit( AudioEventRTS *event ) const
{
	Int limit = event->getAudioEventInfo()->m_limit;
	if (limit == 0) {
		return false;
	}

	Int totalCount = 0;
	Int totalRequestCount = 0;

	std::list<PlayingAudio *>::const_iterator it;
	if (!event->isPositionalAudio()) {
		// 2-D
		for ( it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it ) {
			if ((*it)->m_audioEventRTS->getEventName() == event->getEventName()) {
				if (totalCount == 0) {
					// This is the oldest audio of this type playing.
					event->setHandleToKill((*it)->m_audioEventRTS->getPlayingHandle());
				}
				++totalCount;
			}
		}
	} else {
		// 3-D
		for ( it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it ) {
			if ((*it)->m_audioEventRTS->getEventName() == event->getEventName()) {
				if (totalCount == 0) {
					// This is the oldest audio of this type playing.
					event->setHandleToKill((*it)->m_audioEventRTS->getPlayingHandle());
				}
				++totalCount;
			}
		}
	}

	// Also check the request list in case we've requested to play this sound.
	std::list<AudioRequest*>::const_iterator arIt;
	for (arIt = m_audioRequests.begin(); arIt != m_audioRequests.end(); ++arIt) {
		AudioRequest *req = (*arIt);
		if( req->m_usePendingEvent )
		{
			if( req->m_pendingEvent->getEventName() == event->getEventName() )
			{
				totalRequestCount++;
				totalCount++;
			}
		}
	}

	//If our event is an interrupting type, then normally we would always add it. The exception is when we have requested
	//multiple sounds in the same frame and those requests violate the limit. Because we don't have any "old" sounds to
	//remove in the case of an interrupt, we need to catch it early and prevent the sound from being added if we already
	//reached the limit
	if( event->getAudioEventInfo()->m_control & AC_INTERRUPT )
	{
		if( totalRequestCount < limit )
		{
			Int totalPlayingCount = totalCount - totalRequestCount;
			if( totalRequestCount + totalPlayingCount < limit )
			{
				//We aren't exceeding the actual limit, then clear the kill handle.
				event->setHandleToKill(0);
				return false;
			}

			//We are exceeding the limit - the kill handle will kill the
			//oldest playing sound to enforce the actual limit.
			return false;
		}
	}

	if( totalCount < limit )
	{
		event->setHandleToKill(0);
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::isPlayingAlready( AudioEventRTS *event ) const
{
	std::list<PlayingAudio *>::const_iterator it;
	if (!event->isPositionalAudio()) {
		// 2-D
		for ( it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it ) {
			if ((*it)->m_audioEventRTS->getEventName() == event->getEventName()) {
				return true;
			}
		}
	} else {
		// 3-D
		for ( it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it ) {
			if ((*it)->m_audioEventRTS->getEventName() == event->getEventName()) {
				return true;
			}
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::isObjectPlayingVoice( UnsignedInt objID ) const
{
	if (objID == 0) {
		return false;
	}

	std::list<PlayingAudio *>::const_iterator it;
		// 2-D
	for ( it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it ) {
		if ((*it)->m_audioEventRTS->getObjectID() == objID && (*it)->m_audioEventRTS->getAudioEventInfo()->m_type & ST_VOICE) {
			return true;
		}
	}

	// 3-D
	for ( it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it ) {
		if ((*it)->m_audioEventRTS->getObjectID() == objID && (*it)->m_audioEventRTS->getAudioEventInfo()->m_type & ST_VOICE) {
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
AudioEventRTS* MilesAudioManager::findLowestPrioritySound( AudioEventRTS *event )
{
	AudioPriority priority = event->getAudioEventInfo()->m_priority;
	if( priority == AP_LOWEST )
	{
		//If the event we pass in is the lowest priority, don't bother checking because
		//there is nothing lower priority than lowest.
		return nullptr;
	}
	AudioEventRTS *lowestPriorityEvent = nullptr;
	AudioPriority lowestPriority;

	std::list<PlayingAudio *>::const_iterator it;
	if( event->isPositionalAudio() )
	{
		//3D
		for( it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it )
		{
			AudioEventRTS *itEvent = (*it)->m_audioEventRTS;
			AudioPriority itPriority = itEvent->getAudioEventInfo()->m_priority;
			if( itPriority < priority )
			{
				if( !lowestPriorityEvent || lowestPriority > itPriority )
				{
					lowestPriorityEvent = itEvent;
					lowestPriority = itPriority;
					if( lowestPriority == AP_LOWEST )
					{
						return lowestPriorityEvent;
					}
				}
			}
		}
	}
	else
	{
		//2D
		for( it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it )
		{
			AudioEventRTS *itEvent = (*it)->m_audioEventRTS;
			AudioPriority itPriority = itEvent->getAudioEventInfo()->m_priority;
			if( itPriority < priority )
			{
				if( !lowestPriorityEvent || lowestPriority > itPriority )
				{
					lowestPriorityEvent = itEvent;
					lowestPriority = itPriority;
					if( lowestPriority == AP_LOWEST )
					{
						return lowestPriorityEvent;
					}
				}
			}
		}
	}
	return lowestPriorityEvent;
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::isPlayingLowerPriority( AudioEventRTS *event ) const
{
	//We don't actually want to do anything to this CONST function. Remember, we're
	//just checking to see if there is a lower priority sound.
	AudioPriority priority = event->getAudioEventInfo()->m_priority;
	if( priority == AP_LOWEST )
	{
		//If the event we pass in is the lowest priority, don't bother checking because
		//there is nothing lower priority than lowest.
		return false;
	}
	std::list<PlayingAudio *>::const_iterator it;
	if (!event->isPositionalAudio()) {
		// 2-D
		for ( it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it ) {
			if ((*it)->m_audioEventRTS->getAudioEventInfo()->m_priority < priority) {
				//event->setHandleToKill((*it)->m_audioEventRTS->getPlayingHandle());
				return true;
			}
		}
	} else {
		// 3-D
		for ( it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it ) {
			if ((*it)->m_audioEventRTS->getAudioEventInfo()->m_priority < priority) {
				//event->setHandleToKill((*it)->m_audioEventRTS->getPlayingHandle());
				return true;
			}
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::killLowestPrioritySoundImmediately( AudioEventRTS *event )
{
	//Actually, we want to kill the LOWEST PRIORITY SOUND, not the first "lower" priority
	//sound we find, because it could easily be
	AudioEventRTS *lowestPriorityEvent = findLowestPrioritySound( event );
	if( lowestPriorityEvent )
	{
		std::list<PlayingAudio *>::iterator it;
		if( event->isPositionalAudio() )
		{
			for( it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it )
			{
				PlayingAudio *playing = (*it);

				if( playing->m_audioEventRTS && playing->m_audioEventRTS == lowestPriorityEvent )
				{
					//Release this 3D sound channel immediately because we are going to play another sound in it's place.
					stopPlayingAudio(playing);
					return TRUE;
				}
			}
		}
		else
		{
			for( it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it )
			{
				PlayingAudio *playing = (*it);

				if( playing->m_audioEventRTS && playing->m_audioEventRTS == lowestPriorityEvent )
				{
					//Release this sound channel immediately because we are going to play another sound in it's place.
					stopPlayingAudio(playing);
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}


//-------------------------------------------------------------------------------------------------
void MilesAudioManager::adjustVolumeOfPlayingAudio(AsciiString eventName, Real newVolume)
{
	Real pan;
	std::list<PlayingAudio *>::iterator it;

	PlayingAudio *playing = nullptr;
	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		playing = *it;
		if (playing->m_audioEventRTS->getEventName() == eventName) {
			DEBUG_ASSERTCRASH(playing->m_sample, ("Sample is not expected to be null"));
			playing->m_audioEventRTS->setVolume(newVolume);
			AIL_sample_volume_pan(playing->m_sample, nullptr, &pan);
			AIL_set_sample_volume_pan(playing->m_sample, getEffectiveVolume(playing->m_audioEventRTS), pan);
		}
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
		playing = *it;
		if (playing->m_audioEventRTS->getEventName() == eventName) {
			DEBUG_ASSERTCRASH(playing->m_3DSample, ("3D Sample is not expected to be null"));
			playing->m_audioEventRTS->setVolume(newVolume);
			AIL_set_3D_sample_volume(playing->m_3DSample, getEffectiveVolume(playing->m_audioEventRTS));
		}
	}

	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = *it;
		if (playing->m_audioEventRTS->getEventName() == eventName) {
			DEBUG_ASSERTCRASH(playing->m_stream, ("Stream is not expected to be null"));
			playing->m_audioEventRTS->setVolume(newVolume);
			AIL_stream_volume_pan(playing->m_stream, nullptr, &pan);
			AIL_set_stream_volume_pan(playing->m_stream, getEffectiveVolume(playing->m_audioEventRTS), pan);
		}
	}
}


//-------------------------------------------------------------------------------------------------
void MilesAudioManager::removePlayingAudio( AsciiString eventName )
{
	std::list<PlayingAudio *>::iterator it;

	PlayingAudio *playing = nullptr;
	for( it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it )
	{
		playing = *it;
		if( playing->m_audioEventRTS->getEventName() == eventName )
		{
			stopPlayingAudio(playing);
		}
	}

	for( it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it )
	{
		playing = *it;
		if( playing->m_audioEventRTS->getEventName() == eventName )
		{
			stopPlayingAudio(playing);
		}
	}

	for( it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it )
	{
		playing = *it;
		if( playing->m_audioEventRTS->getEventName() == eventName )
		{
			stopPlayingAudio(playing);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::removeAllDisabledAudio()
{
	std::list<PlayingAudio *>::iterator it;

	PlayingAudio *playing = nullptr;
	for( it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it )
	{
		playing = *it;
		if( playing->m_audioEventRTS->getVolume() == 0.0f )
		{
			stopPlayingAudio(playing);
		}
	}

	for( it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it )
	{
		playing = *it;
		if( playing->m_audioEventRTS->getVolume() == 0.0f )
		{
			stopPlayingAudio(playing);
		}
	}

	for( it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it )
	{
		playing = *it;
		if( playing->m_audioEventRTS->getVolume() == 0.0f )
		{
			stopPlayingAudio(playing);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::processRequestList()
{
	std::list<AudioRequest*>::iterator it;
	for (it = m_audioRequests.begin(); it != m_audioRequests.end(); ) {
		AudioRequest *req = (*it);
		if (!shouldProcessRequestThisFrame(req)) {
			adjustRequest(req);
			++it;
			continue;
		}

		if (!req->m_requiresCheckForSample || checkForSample(req)) {
			processRequest(req);
		}
		deleteInstance(req);
		it = m_audioRequests.erase(it);
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::processPlayingList()
{
	std::list<PlayingAudio *>::iterator it;
	PlayingAudio *playing;

	//
	// Stop audio without locking. Calls into Miles.
	// Update the position of the audio if it is positional.
	//
	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		playing = (*it);

		if (playing->m_status == PS_Stopping) {
			stopPlayingAudio(playing);
			continue;
		}

		if (playing->m_status == PS_Stopped) {
			continue;
		}

		if (m_volumeHasChanged)
		{
			adjustPlayingVolume(playing);
		}
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
		playing = (*it);

		if (playing->m_status == PS_Stopping) {
			stopPlayingAudio(playing);
			continue;
		}

		if (playing->m_status == PS_Stopped) {
			continue;
		}

		if (m_volumeHasChanged)
		{
			adjustPlayingVolume(playing);
		}

		const Coord3D *pos = getCurrentPositionFromEvent(playing->m_audioEventRTS);
		if (pos)
		{
			if( playing->m_audioEventRTS->isDead() )
			{
				stopAudioEvent( playing->m_audioEventRTS->getPlayingHandle() );
			}
			else
			{
				Real volForConsideration = getEffectiveVolume(playing->m_audioEventRTS);
				volForConsideration /= (m_sound3DVolume > 0.0f ? m_soundVolume : 1.0f);
				Bool playAnyways = BitIsSet( playing->m_audioEventRTS->getAudioEventInfo()->m_type, ST_GLOBAL)
					|| playing->m_audioEventRTS->getAudioEventInfo()->m_priority == AP_CRITICAL;
				if( volForConsideration < m_audioSettings->m_minVolume && !playAnyways )
				{
					stopPlayingAudio(playing);
				}
				else
				{
					Real x = pos->x;
					Real y = pos->y;
					Real z = pos->z;
					DEBUG_ASSERTCRASH(playing->m_3DSample, ("3D Sample is not expected to be null"));
					AIL_set_3D_position( playing->m_3DSample, x, y, z );
				}
			}
		}
		else
		{
			stopPlayingAudio(playing);
		}
	}

	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = (*it);

		if (playing->m_status == PS_Stopping) {
			stopPlayingAudio(playing);
			continue;
		}

		if (playing->m_status == PS_Stopped) {
			continue;
		}

		if (m_volumeHasChanged)
		{
			adjustPlayingVolume(playing);
		}
	}

	//
	// Release audio with locking. Must not call into Miles!
	//
	releasePlayingAudioInListIfStopped(m_playingSounds, m_playingSoundsCS);
	releasePlayingAudioInListIfStopped(m_playing3DSounds, m_playing3DSoundsCS);
	releasePlayingAudioInListIfStopped(m_playingStreams, m_playingStreamsCS);

	//
	// Transfer streams to fade list when signaled.
	//
	{
		CriticalSectionClass::LockClass lock(m_playingStreamsCS);
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ) {
			playing = (*it);

			if (playing->m_fade)
			{
				{
					CriticalSectionClass::LockClass lock(m_fadingAudioCS);
					m_fadingAudio.push_back(playing);
				}
				it = m_playingStreams.erase(it);
				continue;
			}

			++it;
		}
	}

	if (m_volumeHasChanged) {
		m_volumeHasChanged = false;
	}
}

//Patch for a rare bug (only on about 5% of in-studio machines suffer, and not all the time) .
//The actual mechanics of this problem are still elusive as of the date of this comment. 8/21/03
//but the cause is clear. Some cinematics do a radical change in the microphone position, which
//calls for a radical 3DSoundVolume adjustment. If this happens while a stereo stream is *ENDING*,
//low-level code gets caught in a tight loop. (Hangs) on some machines.
//To prevent this condition, we just suppress the updating of 3DSoundVolume while one of these
//is on the list. Since the music tracks play continuously, they never *END* during these cinematics.
//so we filter them out as, *NOT SENSITIVE*... we do want to update 3DSoundVolume during music,
//which is almost all of the time.

Bool MilesAudioManager::has3DSensitiveStreamsPlaying() const
{
  if ( m_playingStreams.empty() )
    return FALSE;

	for ( std::list< PlayingAudio* >::const_iterator it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it )
  {
		const PlayingAudio *playing = (*it);

    if ( playing->m_audioEventRTS->getAudioEventInfo()->m_soundType != AT_Music )
    {
      return TRUE;
    }

    if ( playing->m_audioEventRTS->getEventName().startsWith("Game_") == FALSE )
    {
      return TRUE;
    }
  }

  return FALSE;

}


//-------------------------------------------------------------------------------------------------
void MilesAudioManager::processFadingList()
{
	std::list<PlayingAudio *>::iterator it;
	PlayingAudio *playing;

	//
	// Stop audio without locking. Calls into Miles.
	//
	for (it = m_fadingAudio.begin(); it != m_fadingAudio.end(); ++it) {
		playing = (*it);

		if (playing->m_framesFaded >= getAudioSettings()->m_fadeAudioFrames) {
			stopPlayingAudio(playing);
			continue;
		}

		if (playing->m_status == PS_Stopped) {
			continue;
		}

		++playing->m_framesFaded;
		Real volume = getEffectiveVolume(playing->m_audioEventRTS);
		volume *= (1.0f - 1.0f * playing->m_framesFaded / getAudioSettings()->m_fadeAudioFrames);

		switch(playing->m_type)
		{
			case PAT_Sample:
			{
				AIL_set_sample_volume_pan(playing->m_sample, volume, 0.5f);
				break;
			}

			case PAT_3DSample:
			{
				AIL_set_3D_sample_volume(playing->m_3DSample, volume);
				break;
			}

			case PAT_Stream:
			{
				AIL_set_stream_volume_pan(playing->m_stream, volume, 0.5f);
				break;
			}
		}
	}

	//
	// Release audio with locking. Must not call into Miles!
	//
	releasePlayingAudioInListIfStopped(m_fadingAudio, m_fadingAudioCS);
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::shouldProcessRequestThisFrame( AudioRequest *req ) const
{
	if (!req->m_usePendingEvent) {
		return true;
	}

	if (req->m_pendingEvent->getDelay() < MSEC_PER_LOGICFRAME_REAL) {
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::adjustRequest( AudioRequest *req )
{
	if (!req->m_usePendingEvent) {
		return;
	}

	req->m_pendingEvent->decrementDelay(MSEC_PER_LOGICFRAME_REAL);
	req->m_requiresCheckForSample = true;
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::checkForSample( AudioRequest *req )
{
	if (!req->m_usePendingEvent) {
		return true;
	}

  if ( req->m_pendingEvent->getAudioEventInfo() == nullptr )
  {
    // Fill in event info
    getInfoForAudioEvent( req->m_pendingEvent );
  }

	if (req->m_pendingEvent->getAudioEventInfo()->m_type != AT_SoundEffect)
  {
		return true;
	}

	return m_sound->canPlayNow(req->m_pendingEvent);
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::setHardwareAccelerated(Bool accel)
{
	// Extends
	Bool retEarly = (accel == m_hardwareAccel);
	AudioManager::setHardwareAccelerated(accel);

	if (retEarly) {
		return;
	}

	if (m_hardwareAccel) {
		for (Int i = 0; i < MAX_HW_PROVIDERS; ++i) {
			UnsignedInt providerNdx = TheAudio->getProviderIndex(TheAudio->getAudioSettings()->m_preferred3DProvider[i]);
			TheAudio->selectProvider(providerNdx);
			if (getSelectedProvider() == providerNdx) {
				return;
			}
		}
	}

	// set it false
	AudioManager::setHardwareAccelerated(FALSE);
	UnsignedInt providerNdx = TheAudio->getProviderIndex(TheAudio->getAudioSettings()->m_preferred3DProvider[MAX_HW_PROVIDERS]);
	TheAudio->selectProvider(providerNdx);
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::setSpeakerSurround(Bool surround)
{
	// Extends
	Bool retEarly = (surround == m_surroundSpeakers);
	AudioManager::setSpeakerSurround(surround);

	if (retEarly) {
		return;
	}

	UnsignedInt speakerType;
	if (m_surroundSpeakers) {
		speakerType = TheAudio->getAudioSettings()->m_defaultSpeakerType3D;
	} else {
		speakerType = TheAudio->getAudioSettings()->m_defaultSpeakerType2D;
	}

	TheAudio->setSpeakerType(speakerType);
}

//-------------------------------------------------------------------------------------------------
Real MilesAudioManager::getFileLengthMS( AsciiString strToLoad ) const
{
	if (strToLoad.isEmpty()) {
		return 0.0f;
	}

	// Load it as a stream to get the file info without actually opening the file.
	HSTREAM stream = AIL_open_stream(m_digitalHandle, strToLoad.str(), 0);
	if (!stream) {
		return 0.0f;
	}

	long retVal;
	AIL_stream_ms_position(stream, &retVal, nullptr);
	// Now close the stream
	AIL_close_stream(stream);

	return INT_TO_REAL(retVal);
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::closeAnySamplesUsingFile( const void *fileToClose )
{
	std::list<PlayingAudio *>::iterator it;
	PlayingAudio *playing;

	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it ) {
		playing = *it;

		if (playing->m_file == fileToClose) {
			stopPlayingAudio(playing);
		}
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it ) {
		playing = *it;

		if (playing->m_file == fileToClose) {
			stopPlayingAudio(playing);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::setDeviceListenerPosition()
{
	if (m_listener) {
		AIL_set_3D_orientation(m_listener, m_listenerOrientation.x, m_listenerOrientation.y, m_listenerOrientation.z, 0, 0, -1);

		Real x = m_listenerPosition.x;
		Real y = m_listenerPosition.y;
		Real z = m_listenerPosition.z;
		AIL_set_3D_position( m_listener, x, y, z );
	}
}

//-------------------------------------------------------------------------------------------------
const Coord3D *MilesAudioManager::getCurrentPositionFromEvent( AudioEventRTS *event )
{
	if (!event->isPositionalAudio()) {
		return nullptr;
	}

	return event->getCurrentPosition();
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::isOnScreen( const Coord3D *pos ) const
{
	static ICoord2D dummy;
	// WorldToScreen will return True if the point is onscreen and false if it is offscreen.
	return TheTacticalView->worldToScreen(pos, &dummy);
}

//-------------------------------------------------------------------------------------------------
Real MilesAudioManager::getEffectiveVolume(AudioEventRTS *event) const
{
	Real volume = event->getVolume() * event->getVolumeShift();

	switch (event->getAudioEventInfo()->m_soundType)
	{
	case AT_Music:
	{
		volume *= m_musicVolume;
		break;
	}
	case AT_Streaming:
	{
		volume *= m_speechVolume;
		break;
	}
	case AT_SoundEffect:
	{
		if (event->isPositionalAudio())
		{
			volume *= m_sound3DVolume;
			const Coord3D *pos = event->getCurrentPosition();
			if (pos)
			{
				Coord3D distance = m_listenerPosition;
				distance.sub(*pos);
				Real objMinDistance;
				Real objMaxDistance;

				const AudioSettings *audioSettings = TheAudio->getAudioSettings();

				if (event->getAudioEventInfo()->m_type & ST_GLOBAL)
				{
					objMinDistance = audioSettings->m_globalMinRange;
					objMaxDistance = audioSettings->m_globalMaxRange;
				}
				else
				{
					objMinDistance = event->getAudioEventInfo()->m_minDistance;
					objMaxDistance = event->getAudioEventInfo()->m_maxDistance;
				}

				const Real objDistance = distance.length();

				if( objDistance >= objMaxDistance )
				{
					volume = 0.0f;
				}
				else if( audioSettings->m_use3DSoundRangeVolumeFade && objDistance > objMinDistance )
				{
					Real attenuation = (objDistance - objMinDistance) / (objMaxDistance - objMinDistance);
					attenuation = pow(attenuation, audioSettings->m_3DSoundRangeVolumeFadeExponent);
					volume *= 1.0f - attenuation;
				}
			}
		}
		else
		{
			volume *= m_soundVolume;
		}
		break;
	}
	}

	return volume;
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::startNextLoop( PlayingAudio *looping )
{
	closeFile(looping->m_file);
	looping->m_file = nullptr;

	if (looping->m_status != PS_Playing) {
		return false;
	}

	if (looping->m_audioEventRTS->hasMoreLoops()) {
		// generate a new filename, and test to see whether we can play with it now
		looping->m_audioEventRTS->generateFilename();

		if (looping->m_audioEventRTS->getDelay() > MSEC_PER_LOGICFRAME_REAL) {
			// fake it out so that this sound appears done, but also so that it will not
			// delete the sound on completion (which would suck)
			looping->m_cleanupAudioEventRTS = false;
			InterlockedCompareExchange(reinterpret_cast<volatile long*>(&looping->m_status), PS_Stopping, PS_Playing);

			AudioRequest *req = allocateAudioRequest(true);
			req->m_pendingEvent = looping->m_audioEventRTS;
			req->m_requiresCheckForSample = true;
			appendAudioRequest(req);
			return true;
		}

		if (looping->m_type == PAT_3DSample) {
			looping->m_file = playSample3D(looping->m_audioEventRTS, looping->m_3DSample);
		} else {
			looping->m_file = playSample(looping->m_audioEventRTS, looping->m_sample);
		}

		return looping->m_file != nullptr;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::playStream( AudioEventRTS *event, HSTREAM stream )
{
	// Force it to the beginning
	if (event->getAudioEventInfo()->m_soundType == AT_Music) {
		AIL_set_stream_loop_count(stream, INFINITE_LOOP_COUNT);
	}

	AIL_register_stream_callback(stream, setStreamCompleted);
	AIL_start_stream(stream);
	if (event->getAudioEventInfo()->m_soundType == AT_Music) {
		// Need to stop/fade out the old music here.
	}
}

//-------------------------------------------------------------------------------------------------
void *MilesAudioManager::playSample( AudioEventRTS *event, HSAMPLE sample )
{
	AIL_init_sample(sample);

	// Prep any sort of filtering, etc, here
	AIL_register_EOS_callback(sample, setSampleCompleted);
	initFilters(sample, event);

	// Load the file in
	void *fileBuffer = nullptr;
	fileBuffer = loadFileForRead(event);
	if (fileBuffer) {
		AIL_set_sample_file(sample, fileBuffer, 0);

		// Start playback
		AIL_start_sample(sample);
	}

	return fileBuffer;
}

//-------------------------------------------------------------------------------------------------
void *MilesAudioManager::playSample3D( AudioEventRTS *event, H3DSAMPLE sample3D )
{
	const Coord3D *pos = getCurrentPositionFromEvent(event);
	if (pos) {
		// Load the file in
		void *fileBuffer = loadFileForRead(event);
		if (fileBuffer) {
			AIL_set_3D_sample_file(sample3D, fileBuffer);

			// Prep any sort of filtering, etc, here
			AIL_register_3D_EOS_callback(sample3D, set3DSampleCompleted);

			// Set the position values of the sample here
			if (event->getAudioEventInfo()->m_type & ST_GLOBAL) {
				AIL_set_3D_sample_distances(sample3D, TheAudio->getAudioSettings()->m_globalMinRange, TheAudio->getAudioSettings()->m_globalMaxRange );
			} else {
				AIL_set_3D_sample_distances(sample3D, event->getAudioEventInfo()->m_minDistance, event->getAudioEventInfo()->m_maxDistance );
			}

			// Set the position of the sample here
			Real x = pos->x;
			Real y = pos->y;
			Real z = pos->z;
			AIL_set_3D_position( sample3D, x, y, z );
			initFilters3D(sample3D, event, pos);

			// Start playback
			AIL_start_3D_sample(sample3D);
		}
		return fileBuffer;
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::buildProviderList()
{
   HPROENUM next = HPROENUM_FIRST;

	 char *name;
	 UnsignedInt index = 0;
	 while (index < MAXPROVIDERS && AIL_enumerate_3D_providers(&next, &m_provider3D[index].id, &name)) {
		 m_provider3D[index].name.set(name);	// set it to the AsciiString
		 ++index;
	 }

	 m_providerCount = index;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::createListener()
{
	if (!(isOn(AudioAffect_Sound3D) && isValidProvider())) {
		return;
	}

	m_listener = AIL_open_3D_listener(m_provider3D[m_selectedProvider].id);
	// initial listener position will be (0, 0, 0)
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::initDelayFilter()
{
	if (m_delayFilter != nullptr) {
		return;
	}

	char* filterName;
	HPROENUM enumFLTs = HPROENUM_FIRST;
	HPROVIDER currentProvider;

	while (AIL_enumerate_filters(&enumFLTs, &currentProvider, &filterName )) {
		if (strcmp(filterName,"Mono Delay Filter") == 0) {
			m_delayFilter = currentProvider;
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool MilesAudioManager::isValidProvider()
{
	return (m_selectedProvider < m_providerCount);
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::initSamplePools()
{
	if (!(isOn(AudioAffect_Sound3D) && isValidProvider())) {
		return;
	}

	m_availableSamples.reserve(getAudioSettings()->m_sampleCount2D);
	m_available3DSamples.reserve(getAudioSettings()->m_sampleCount3D);

	int i = 0;
	for (i = 0; i < getAudioSettings()->m_sampleCount2D; ++i) {
		HSAMPLE sample = AIL_allocate_sample_handle(m_digitalHandle);
		DEBUG_ASSERTCRASH(sample, ("Couldn't get %d 2D samples", i + 1));
		if (sample) {
			AIL_init_sample(sample);
			AIL_set_sample_user_data(sample, 0, (void *)(i + 1));
			m_availableSamples.push_back(sample);
			++m_num2DSamples;
		}
	}

	for (i = 0; i < getAudioSettings()->m_sampleCount3D; ++i) {
		H3DSAMPLE sample = AIL_allocate_3D_sample_handle(m_provider3D[m_selectedProvider].id);
		DEBUG_ASSERTCRASH(sample, ("Couldn't get %d 3D samples", i + 1));
		if (sample) {
			AIL_set_3D_user_data(sample, 0, (void *)(i + 1));
			m_available3DSamples.push_back(sample);
			++m_num3DSamples;
		}
	}

	// Streams are basically free, so we can just allocate the appropriate number
	m_numStreams = getAudioSettings()->m_streamCount;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::processRequest( AudioRequest *req )
{
	switch (req->m_request)
	{
		case AR_Play:
		{
			playAudioEvent(req->m_pendingEvent);
			break;
		}
		case AR_Pause:
		{
			pauseAudioEvent(req->m_handleToInteractOn);
			break;
		}
		case AR_Stop:
		{
			stopAudioEvent(req->m_handleToInteractOn);
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void *MilesAudioManager::getHandleForBink()
{
	if (m_binkHandle == nullptr) {
		PlayingAudio *aud = allocatePlayingAudio();
		aud->m_audioEventRTS = NEW AudioEventRTS("BinkHandle");	// poolify
		getInfoForAudioEvent(aud->m_audioEventRTS);
		aud->m_sample = getAvailable2DSample(aud->m_audioEventRTS);
		aud->m_type = PAT_Sample;

		if (!aud->m_sample) {
			stopPlayingAudio(aud);
			releasePlayingAudio(aud);
			return nullptr;
		}

		m_binkHandle = aud;
	}

	AILLPDIRECTSOUND lpDS;
	AIL_get_DirectSound_info(m_binkHandle->m_sample, &lpDS, nullptr);
	return lpDS;
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::releaseHandleForBink()
{
	if (m_binkHandle) {
		stopPlayingAudio(m_binkHandle);
		releasePlayingAudio(m_binkHandle);
		m_binkHandle = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
void MilesAudioManager::friend_forcePlayAudioEventRTS(const AudioEventRTS* eventToPlay)
{
	if (!eventToPlay->getAudioEventInfo()) {
		getInfoForAudioEvent(eventToPlay);
		if (!eventToPlay->getAudioEventInfo()) {
			DEBUG_CRASH(("No info for forced audio event '%s'", eventToPlay->getEventName().str()));
			return;
		}
	}

	switch (eventToPlay->getAudioEventInfo()->m_soundType)
	{
		case AT_Music:
			if (!isOn(AudioAffect_Music))
				return;
			break;
		case AT_SoundEffect:
			if (!isOn(AudioAffect_Sound) || !isOn(AudioAffect_Sound3D))
				return;
			break;
		case AT_Streaming:
			if (!isOn(AudioAffect_Speech))
				return;
			break;
	}

	AudioEventRTS event = *eventToPlay;

	event.generateFilename();
	event.generatePlayInfo();

	std::list<std::pair<AsciiString, Real>/**/>::iterator it;
	for (it = m_adjustedVolumes.begin(); it != m_adjustedVolumes.end(); ++it) {
		if (it->first == event.getEventName()) {
			event.setVolume(it->second);
			break;
		}
	}

	AsciiString fileToPlay = event.getFilename();
	HAUDIO haud = AIL_quick_load_and_play(fileToPlay.str(), 1, 0);

	// Even though the event type is not Speech, this is used only for mission briefings, so use the
	// speech slider to adjust the volume.
	// Get the volume from the event, and pass 0.5 to play the audio in the middle. (0.0 is full left, 1.0 is full right)
	AIL_quick_set_volume(haud, event.getVolume() * getVolume(AudioAffect_Speech), 0.5);
	m_audioForcePlayed.push_back(haud);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void AILCALLBACK setSampleCompleted( HSAMPLE sampleCompleted )
{
	TheAudio->notifyOfAudioCompletion((UnsignedInt) sampleCompleted, PAT_Sample);
}

//-------------------------------------------------------------------------------------------------
void AILCALLBACK set3DSampleCompleted( H3DSAMPLE sample3DCompleted )
{
	TheAudio->notifyOfAudioCompletion((UnsignedInt) sample3DCompleted, PAT_3DSample);
}

//-------------------------------------------------------------------------------------------------
void AILCALLBACK setStreamCompleted( HSTREAM streamCompleted )
{
	TheAudio->notifyOfAudioCompletion((UnsignedInt) streamCompleted, PAT_Stream);
}

//-------------------------------------------------------------------------------------------------
U32 AILCALLBACK streamingFileOpen(char const *fileName, void **file_handle)
{
#if defined(RTS_DEBUG)
	if (sizeof(U32) != sizeof(File*)) {
		RELEASE_CRASH(("streamingFileOpen - This function requires work in order to compile on non 32-bit platforms."));
	}
#endif

	(*file_handle) = (void *) TheFileSystem->openFile(fileName, File::READ | File::STREAMING);
	return ((*file_handle) != nullptr);
}

//-------------------------------------------------------------------------------------------------
void AILCALLBACK streamingFileClose(void *fileHandle)
{
	((File*) fileHandle)->close();
}

//-------------------------------------------------------------------------------------------------
S32 AILCALLBACK streamingFileSeek(void *fileHandle, S32 offset, U32 type)
{
	return ((File*) fileHandle)->seek(offset, (File::seekMode) type);
}

//-------------------------------------------------------------------------------------------------
U32 AILCALLBACK streamingFileRead(void *file_handle, void *buffer, U32 bytes)
{
	return ((File*) file_handle)->read(buffer, bytes);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
AudioFileCache::AudioFileCache() : m_maxSize(0), m_currentlyUsedSize(0), m_mutexName("AudioFileCacheMutex")
{
	m_mutex = CreateMutex(nullptr, FALSE, m_mutexName);
}

//-------------------------------------------------------------------------------------------------
AudioFileCache::~AudioFileCache()
{
	{
		ScopedMutex mut(m_mutex);

		// Free all the samples that are open.
		OpenFilesHashIt it;
		for ( it = m_openFiles.begin(); it != m_openFiles.end(); ++it ) {
			if (it->second.m_openCount > 0) {
				DEBUG_CRASH(("Sample '%s' is still playing, and we're trying to quit.", it->second.m_eventInfo->m_audioName.str()));
			}

			releaseOpenAudioFile(&it->second);
			// Don't erase it from the map, cause it makes this whole process way more complicated, and
			// we're about to go away anyways.
		}
	}

	CloseHandle(m_mutex);
}

//-------------------------------------------------------------------------------------------------
void *AudioFileCache::openFile( AudioEventRTS *eventToOpenFrom )
{
	// Protect the entire openFile function
	ScopedMutex mut(m_mutex);

	AsciiString strToFind;
	switch (eventToOpenFrom->getNextPlayPortion())
	{
		case PP_Attack:
			strToFind = eventToOpenFrom->getAttackFilename();
			break;
		case PP_Sound:
			strToFind = eventToOpenFrom->getFilename();
			break;
		case PP_Decay:
			strToFind = eventToOpenFrom->getDecayFilename();
			break;
		case PP_Done:
			return nullptr;
	}

	OpenFilesHash::iterator it;
	it = m_openFiles.find(strToFind);

	if (it != m_openFiles.end()) {
		++it->second.m_openCount;
		return it->second.m_file;
	}

	// Couldn't find the file, so actually open it.
	File *file = TheFileSystem->openFile(strToFind.str());
	if (!file) {
		DEBUG_ASSERTLOG(strToFind.isEmpty(), ("Missing Audio File: '%s'", strToFind.str()));
		return nullptr;
	}

	UnsignedInt fileSize = file->size();
	char* buffer = file->readEntireAndClose();

	OpenAudioFile openedAudioFile;
	openedAudioFile.m_eventInfo = eventToOpenFrom->getAudioEventInfo();

	AILSOUNDINFO soundInfo;
	AIL_WAV_info(buffer, &soundInfo);

	if (eventToOpenFrom->isPositionalAudio()) {
		if (soundInfo.channels > 1) {
			DEBUG_CRASH(("Requested Positional Play of audio '%s', but it is in stereo.", strToFind.str()));
			delete [] buffer;
			return nullptr;
		}
	}

	if (soundInfo.format == WAVE_FORMAT_IMA_ADPCM) {
		void *decompressFileBuffer;
		U32 newFileSize;
		AIL_decompress_ADPCM(&soundInfo, &decompressFileBuffer, &newFileSize);
		fileSize = newFileSize;
		openedAudioFile.m_compressed = TRUE;
		delete [] buffer;
		openedAudioFile.m_file = decompressFileBuffer;
		openedAudioFile.m_soundInfo = soundInfo;
		openedAudioFile.m_openCount = 1;
	} else if (soundInfo.format == WAVE_FORMAT_PCM) {
		openedAudioFile.m_compressed = FALSE;
		openedAudioFile.m_file = buffer;
		openedAudioFile.m_soundInfo = soundInfo;
		openedAudioFile.m_openCount = 1;
	} else {
		DEBUG_CRASH(("Unexpected compression type in '%s'", strToFind.str()));
		// prevent leaks
		delete [] buffer;
		return nullptr;
	}

	openedAudioFile.m_fileSize = fileSize;
	m_currentlyUsedSize += openedAudioFile.m_fileSize;
	if (m_currentlyUsedSize > m_maxSize) {
		// We need to free some samples, or we're not going to be able to play this sound.
		if (!freeEnoughSpaceForSample(openedAudioFile)) {
			m_currentlyUsedSize -= openedAudioFile.m_fileSize;
			releaseOpenAudioFile(&openedAudioFile);
			return nullptr;
		}
	}

	m_openFiles[strToFind] = openedAudioFile;
	return openedAudioFile.m_file;
}

//-------------------------------------------------------------------------------------------------
void AudioFileCache::closeFile( void *fileToClose )
{
	if (!fileToClose) {
		return;
	}

	// Protect the entire closeFile function
	ScopedMutex mut(m_mutex);

	OpenFilesHash::iterator it;
	for ( it = m_openFiles.begin(); it != m_openFiles.end(); ++it ) {
		if ( it->second.m_file == fileToClose ) {
			--it->second.m_openCount;
			return;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void AudioFileCache::setMaxSize( UnsignedInt size )
{
	// Protect the function, in case we're trying to use this value elsewhere.
	ScopedMutex mut(m_mutex);

	m_maxSize = size;
}

//-------------------------------------------------------------------------------------------------
void AudioFileCache::releaseOpenAudioFile( OpenAudioFile *fileToRelease )
{
	if (fileToRelease->m_openCount > 0) {
		// This thing needs to be terminated IMMEDIATELY.
		TheAudio->closeAnySamplesUsingFile(fileToRelease->m_file);
	}

	if (fileToRelease->m_file) {
		if (fileToRelease->m_compressed) {
			// Files read in via AIL_decompress_ADPCM must be freed with AIL_mem_free_lock.
			AIL_mem_free_lock(fileToRelease->m_file);
		} else {
			// Otherwise, we read it, we own it, blow it away.
			delete [] fileToRelease->m_file;
		}
		fileToRelease->m_file = nullptr;
		fileToRelease->m_eventInfo = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
Bool AudioFileCache::freeEnoughSpaceForSample(const OpenAudioFile& sampleThatNeedsSpace)
{

	Int spaceRequired = m_currentlyUsedSize - m_maxSize;
	Int runningTotal = 0;

	std::list<AsciiString> filesToClose;
	// First, search for any samples that have ref counts of 0. They are low-hanging fruit, and
	// should be considered immediately.
	OpenFilesHashIt it;
	for (it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
		if (it->second.m_openCount == 0) {
			// This is said low-hanging fruit.
			filesToClose.push_back(it->first);

			runningTotal += it->second.m_fileSize;

			if (runningTotal >= spaceRequired) {
				break;
			}
		}
	}

	// If we don't have enough space yet, then search through the events who have a count of 1 or more
	// and who are lower priority than this sound.
	// Mical said that at this point, sounds shouldn't care if other sounds are interruptable or not.
	// Kill any files of lower priority necessary to clear our the buffer.
	if (runningTotal < spaceRequired) {
		for (it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
			if (it->second.m_openCount > 0) {
				if (it->second.m_eventInfo->m_priority < sampleThatNeedsSpace.m_eventInfo->m_priority) {
					filesToClose.push_back(it->first);
					runningTotal += it->second.m_fileSize;

					if (runningTotal >= spaceRequired) {
						break;
					}
				}
			}
		}
	}

	// We weren't able to find enough sounds to truncate. Therefore, this sound is not going to play.
	if (runningTotal < spaceRequired) {
		return FALSE;
	}

	std::list<AsciiString>::iterator ait;
	for (ait = filesToClose.begin(); ait != filesToClose.end(); ++ait) {
		OpenFilesHashIt itToErase = m_openFiles.find(*ait);
		if (itToErase != m_openFiles.end()) {
			releaseOpenAudioFile(&itToErase->second);
			m_currentlyUsedSize -= itToErase->second.m_fileSize;
			m_openFiles.erase(itToErase);
		}
	}

	return TRUE;
}


#if defined(RTS_DEBUG)
//-------------------------------------------------------------------------------------------------
void MilesAudioManager::dumpAllAssetsUsed()
{
	if (!TheGlobalData->m_preloadReport) {
		return;
	}

	// Dump all the audio assets we've used.
	FILE *logfile=fopen("PreloadedAssets.txt","a+");	//append to log
	if (!logfile)
		return;

	std::list<AsciiString> missingEvents;
	std::list<AsciiString> usedFiles;

	std::list<AsciiString>::iterator lit;

	fprintf(logfile, "\nAudio Asset Report - BEGIN\n");
	{
		SetAsciiStringIt it;
		std::vector<AsciiString>::iterator asIt;
		for (it = m_allEventsLoaded.begin(); it != m_allEventsLoaded.end(); ++it) {
			AsciiString astr = *it;
			AudioEventInfo *aei = findAudioEventInfo(astr);
			if (!aei) {
				missingEvents.push_back(astr);
				continue;
			}

			for (asIt = aei->m_attackSounds.begin(); asIt != aei->m_attackSounds.end(); ++asIt) {
				usedFiles.push_back(*asIt);
			}

			for (asIt = aei->m_sounds.begin(); asIt != aei->m_sounds.end(); ++asIt) {
				usedFiles.push_back(*asIt);
			}

			for (asIt = aei->m_decaySounds.begin(); asIt != aei->m_decaySounds.end(); ++asIt) {
				usedFiles.push_back(*asIt);
			}

			if (!aei->m_filename.isEmpty()) {
				usedFiles.push_back(aei->m_filename);
			}
		}

		fprintf(logfile, "\nEvents Requested that are missing information - BEGIN\n");
		for (lit = missingEvents.begin(); lit != missingEvents.end(); ++lit) {
			fprintf(logfile, "%s\n", (*lit).str());
		}
		fprintf(logfile, "\nEvents Requested that are missing information - END\n");

		fprintf(logfile, "\nFiles Used - BEGIN\n");
		for (lit = usedFiles.begin(); lit != usedFiles.end(); ++lit) {
			fprintf(logfile, "%s\n", (*lit).str());
		}
		fprintf(logfile, "\nFiles Used - END\n");
	}
	fprintf(logfile, "\nAudio Asset Report - END\n");
	fclose(logfile);
	logfile = nullptr;
}
#endif
