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

// FILE: OpenALAudioManager.cpp 
/*---------------------------------------------------------------------------*/
/* EA Pacific                                                                */
/* Confidential Information	                                                 */
/* Copyright (C) 2001 - All Rights Reserved                                  */
/* DO NOT DISTRIBUTE                                                         */
/*---------------------------------------------------------------------------*/
/* Project:    RTS3                                                          */
/* File name:  OpenALAudioManager.cpp                                         */
/* Created:    Stephan Vedder, 3/9/2025s                              */
/* Desc:       This is the implementation for the OpenALAudioManager, which   */
/*						 interfaces with the Miles Sound System.                       */
/* Revision History:                                                         */
/*		3/9/2025 : Initial creation                                           */
/*---------------------------------------------------------------------------*/

#include "Lib/BaseType.h"
#include "OpenALAudioDevice/OpenALAudioManager.h"
#include "OpenALAudioDevice/OpenALAudioStream.h"
#include "OpenALAudioCache.h"

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

#include "GameClient/DebugDisplay.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/VideoPlayer.h"
#include "GameClient/View.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/TerrainLogic.h"
// GeneralsX @bugfix fbraz3 24/06/2026 Save and restore FPU precision mode when calling audio manager entrypoints.
#include "GameLogic/FPUControl.h"

#include "Common/file.h"

#include <AL/alext.h>

// GeneralsX @bugfix 14/06/2026 Self-heal for stuck "disallow speech" flag.
// Uninterruptible streamed speech (e.g. Generals Challenge enemy taunts) sets
// disallowSpeech=TRUE so a speaker doesn't talk over himself; it's cleared when
// the stream is detected stopped. But a finished finite-speech stream can linger
// "not stopped" for a long time (its drained source keeps getting restarted in
// OpenALAudioStream::update()), so the flag stays stuck and every subsequent
// taunt is rejected with AHSV_NoSound — the player hears only the first taunt.
// We record the frame the flag was set and force-clear it after longer than any
// real voice line can last, so a genuinely-playing taunt is never cut off but a
// stale flag can't silence the rest of the match.
static Int s_disallowSpeechSetFrame = 0;
// 30 logic frames/sec; 15s comfortably exceeds the longest taunt/EVA line.
static const Int DISALLOW_SPEECH_MAX_FRAMES = 30 * 15;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

#ifdef _INTERNAL
//#pragma optimize("", off)
//#pragma MESSAGE("************************************** WARNING, optimization disabled for debugging purposes")
#endif

enum { INFINITE_LOOP_COUNT = 1000000 };

#define LOAD_ALC_PROC(N) N = reinterpret_cast<decltype(N)>(alcGetProcAddress(m_alcDevice, #N))

static inline bool sourceIsStopped(ALuint source)
{
	ALenum state;
	alGetSourcei(source, AL_SOURCE_STATE, &state);
	
	return (state == AL_STOPPED);
}

//-------------------------------------------------------------------------------------------------
OpenALAudioManager::OpenALAudioManager() :
	m_providerCount(1),
	m_selectedProvider(PROVIDER_ERROR),
	m_selectedSpeakerType(0),
	m_lastProvider(PROVIDER_ERROR),
	m_alcDevice(NULL),
	m_alcContext(NULL),
	m_num2DSamples(0),
	m_num3DSamples(0),
	m_numStreams(0),
	m_binkAudio(NULL),
	m_pref3DProvider(AsciiString::TheEmptyString),
	m_prefSpeaker(AsciiString::TheEmptyString)
{
	m_audioCache = NEW OpenALAudioFileCache;
	m_provider3D[0].name = "Miles Fast 2D Positional Audio";
	m_provider3D[0].m_isValid = true;
}

//-------------------------------------------------------------------------------------------------
OpenALAudioManager::~OpenALAudioManager()
{
	DEBUG_ASSERTCRASH(m_binkAudio == NULL, ("Leaked a Bink handle. Chuybregts"));
	releaseHandleForBink();
	closeDevice();
	delete m_audioCache;

	DEBUG_ASSERTCRASH(this == TheAudio, ("Umm...\n"));
	TheAudio = NULL;
}

//-------------------------------------------------------------------------------------------------
#if defined(_DEBUG) || defined(_INTERNAL)
AudioHandle OpenALAudioManager::addAudioEvent(const AudioEventRTS* eventToAdd)
{
	ScopedFPUGuard fpuGuard;
	if (TheGlobalData->m_preloadReport) {
		if (!eventToAdd->getEventName().isEmpty()) {
			m_allEventsLoaded.insert(eventToAdd->getEventName());
		}
	}

	return AudioManager::addAudioEvent(eventToAdd);
}
#endif

#if defined(_DEBUG) || defined(_INTERNAL)
//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::audioDebugDisplay(DebugDisplayInterface* dd, void*, FILE* fp)
{
	std::list<PlayingAudio*>::iterator it;

	static char buffer[128] = { 0 };
	if (buffer[0] == 0) {
        strncpy(buffer, alGetString(AL_VERSION), sizeof(buffer));
	}

	Coord3D lookPos;
	TheTacticalView->getPosition(&lookPos);
	lookPos.z = TheTerrainLogic->getGroundHeight(lookPos.x, lookPos.y);
	const Coord3D* mikePos = TheAudio->getListenerPosition();
	Coord3D distanceVector = TheTacticalView->get3DCameraPosition();
	distanceVector.sub(mikePos);

	Int now = TheGameLogic->getFrame();
	static Int lastCheck = now;
	//const Int frames = 60;
	static Int latency = 0;
	static Int worstLatency = 0;

	if (dd)
	{
		dd->printf("OpenAL version: %s    ", buffer);
		dd->printf("Memory Usage : %d/%d\n", m_audioCache->getCurrentlyUsedSize(), m_audioCache->getMaxSize());
		dd->printf("Sound: %s    ", (isOn(AudioAffect_Sound) ? "Yes" : "No"));
		dd->printf("3DSound: %s    ", (isOn(AudioAffect_Sound3D) ? "Yes" : "No"));
		dd->printf("Speech: %s    ", (isOn(AudioAffect_Speech) ? "Yes" : "No"));
		dd->printf("Music: %s\n", (isOn(AudioAffect_Music) ? "Yes" : "No"));
		dd->printf("Channels Available: ");
		dd->printf("%d Sounds    ", m_sound->getAvailableSamples());

		dd->printf("%d 3D Sounds\n", m_sound->getAvailable3DSamples());
		dd->printf("Volume: ");
		dd->printf("Sound: %d    ", REAL_TO_INT(m_soundVolume * 100.0f));
		dd->printf("3DSound: %d    ", REAL_TO_INT(m_sound3DVolume * 100.0f));
		dd->printf("Speech: %d    ", REAL_TO_INT(m_speechVolume * 100.0f));
		dd->printf("Music: %d\n", REAL_TO_INT(m_musicVolume * 100.0f));
		dd->printf("Current 3D Provider: %s    ",

			TheAudio->getProviderName(m_selectedProvider).str());
		dd->printf("Current Speaker Type: %s\n", TheAudio->translateUnsignedIntToSpeakerType(TheAudio->getSpeakerType()).str());

		dd->printf("Looking at: (%d,%d,%d) -- Microphone at: (%d,%d,%d)\n",
			(Int)lookPos.x, (Int)lookPos.y, (Int)lookPos.z, (Int)mikePos->x, (Int)mikePos->y, (Int)mikePos->z);
		dd->printf("Camera distance from microphone: %d -- Zoom Volume: %d%%\n",
			(Int)distanceVector.length(), (Int)(TheAudio->getZoomVolume() * 100.0f));
		dd->printf("Worst latency: %d -- Current latency: %d\n", worstLatency, latency);

		dd->printf("-----------------------------------------------------------\n");
		dd->printf("Playing Audio\n");
	}
	if (fp)
	{
		fprintf(fp, "Miles Sound System version: %s    ", buffer);
		fprintf(fp, "Memory Usage : %d/%d\n", m_audioCache->getCurrentlyUsedSize(), m_audioCache->getMaxSize());
		fprintf(fp, "Sound: %s    ", (isOn(AudioAffect_Sound) ? "Yes" : "No"));
		fprintf(fp, "3DSound: %s    ", (isOn(AudioAffect_Sound3D) ? "Yes" : "No"));
		fprintf(fp, "Speech: %s    ", (isOn(AudioAffect_Speech) ? "Yes" : "No"));
		fprintf(fp, "Music: %s\n", (isOn(AudioAffect_Music) ? "Yes" : "No"));
		fprintf(fp, "Channels Available: ");
		fprintf(fp, "%d Sounds    ", m_sound->getAvailableSamples());
		fprintf(fp, "%d 3D Sounds\n", m_sound->getAvailable3DSamples());
		fprintf(fp, "Volume: ");
		fprintf(fp, "Sound: %d    ", REAL_TO_INT(m_soundVolume * 100.0f));
		fprintf(fp, "3DSound: %d    ", REAL_TO_INT(m_sound3DVolume * 100.0f));
		fprintf(fp, "Speech: %d    ", REAL_TO_INT(m_speechVolume * 100.0f));
		fprintf(fp, "Music: %d\n", REAL_TO_INT(m_musicVolume * 100.0f));
		fprintf(fp, "Current 3D Provider: %s    ", TheAudio->getProviderName(m_selectedProvider).str());
		fprintf(fp, "Current Speaker Type: %s\n", TheAudio->translateUnsignedIntToSpeakerType(TheAudio->getSpeakerType()).str());

		fprintf(fp, "Looking at: (%d,%d,%d) -- Microphone at: (%d,%d,%d)\n",
			(Int)lookPos.x, (Int)lookPos.y, (Int)lookPos.z, (Int)mikePos->x, (Int)mikePos->y, (Int)mikePos->z);
		fprintf(fp, "Camera distance from microphone: %d -- Zoom Volume: %d%%\n",
			(Int)distanceVector.length(), (Int)(TheAudio->getZoomVolume() * 100.0f));

		fprintf(fp, "-----------------------------------------------------------\n");
		fprintf(fp, "Playing Audio\n");
	}

	PlayingAudio* playing = NULL;
	Int channel;
	Int channelCount;
	Real volume = 0.0f;
	AsciiString filenameNoSlashes;

	const Int maxChannels = 64;
	PlayingAudio* playingArray[maxChannels] = { NULL };

	// 2-D Sounds
	if (dd)
	{
		dd->printf("-----------------------------------------------------Sounds\n");
		channelCount = TheAudio->getNum2DSamples();
		channel = 1;
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			playing = *it;
			if (!playing) {
				continue;
			}

			playingArray[channel] = playing;
			channel++;
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
			playingArray[i] = NULL;
		}
	}
	if (fp)
	{
		fprintf(fp, "-----------------------------------------------------Sounds\n");
		channelCount = TheAudio->getNum2DSamples();
		channel = 1;
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it)
		{
			playing = *it;
			if (!playing)
			{
				continue;
			}
			filenameNoSlashes = playing->m_audioEventRTS->getFilename();
			filenameNoSlashes = filenameNoSlashes.reverseFind('\\') + 1;

			// Calculate Sample volume
			volume = 100.0f;
			volume *= getEffectiveVolume(playing->m_audioEventRTS);

			fprintf(fp, "%2d: %-20s - (%s) Volume: %d (2D)\n", channel++, playing->m_audioEventRTS->getEventName().str(), filenameNoSlashes.str(), REAL_TO_INT(volume));
		}
		for (int i = channel; i <= channelCount; ++i)
		{
			fprintf(fp, "%d: Silence\n", i);
		}
	}

	const Coord3D* microphonePos = TheAudio->getListenerPosition();

	// Now 3D Sounds
	if (dd)
	{
		dd->printf("--------------------------------------------------3D Sounds\n");
		channelCount = TheAudio->getNum3DSamples();
		channel = 1;
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			playing = *it;
			if (!playing) {
				continue;
			}

			playingArray[channel] = playing;
			channel++;
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
			const Coord3D* pos = playing->m_audioEventRTS->getPosition();
			char distStr[32];
			if (pos)
			{
				Coord3D vector = *microphonePos;
				vector.sub(pos);
				dist = vector.length();
				sprintf(distStr, "%d", REAL_TO_INT(dist));
			}
			else
			{
				sprintf(distStr, "???");
			}
			char str[32];
			switch (playing->m_audioEventRTS->getOwnerType())
			{
			case OT_Positional:
				sprintf(str, "(3D)");
				break;
			case OT_Object:
				sprintf(str, "(3DObj)");
				break;
			case OT_Drawable:
				sprintf(str, "(3DDraw)");
				break;
			case OT_Dead:
				sprintf(str, "(3DDead)");
				break;

			}

			dd->printf("%2d: %-20s - (%s) Volume: %d, Dist: %s, %s\n",
				i, playing->m_audioEventRTS->getEventName().str(), filenameNoSlashes.str(), REAL_TO_INT(volume), distStr, str);
			playingArray[i] = NULL;
		}
	}
	if (fp)
	{
		fprintf(fp, "--------------------------------------------------3D Sounds\n");
		channelCount = TheAudio->getNum3DSamples();
		channel = 1;
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it)
		{
			playing = *it;
			if (!playing)
			{
				continue;
			}
			filenameNoSlashes = playing->m_audioEventRTS->getFilename();
			filenameNoSlashes = filenameNoSlashes.reverseFind('\\') + 1;

			// Calculate Sample volume
			volume = 100.0f;
			volume *= getEffectiveVolume(playing->m_audioEventRTS);
			fprintf(fp, "%2d: %-24s - (%s) Volume: %d \n", channel++, playing->m_audioEventRTS->getEventName().str(), filenameNoSlashes.str(), REAL_TO_INT(volume));
		}

		for (int i = channel; i <= channelCount; ++i)
		{
			fprintf(fp, "%2d: Silence\n", i);
		}
	}

	// Now Streams
	if (dd)
	{
		dd->printf("----------------------------------------------------Streams\n");
		channelCount = TheAudio->getNumStreams();
		channel = 1;
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
			playing = *it;
			if (!playing) {
				continue;
			}
			filenameNoSlashes = playing->m_audioEventRTS->getFilename();
			filenameNoSlashes = filenameNoSlashes.reverseFind('\\') + 1;


			// Calculate Sample volume
			volume = 100.0f;
			volume *= getEffectiveVolume(playing->m_audioEventRTS);

			dd->printf("%2d: %-24s - (%s)  Volume: %d (Stream)\n", channel++, playing->m_audioEventRTS->getEventName().str(), filenameNoSlashes.str(), REAL_TO_INT(volume));
		}

		for (int i = channel; i <= channelCount; ++i) {
			dd->printf("%2d: Silence\n", i);
		}
		dd->printf("===========================================================\n");
	}
	if (fp)
	{
		fprintf(fp, "----------------------------------------------------Streams\n");
		channelCount = TheAudio->getNumStreams();
		channel = 1;
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it)
		{
			playing = *it;
			if (!playing)
			{
				continue;
			}
			filenameNoSlashes = playing->m_audioEventRTS->getFilename();
			filenameNoSlashes = filenameNoSlashes.reverseFind('\\') + 1;

			// Calculate Sample volume
			volume = 100.0f;
			volume *= getEffectiveVolume(playing->m_audioEventRTS);

			fprintf(fp, "%2d: %-24s - (%s)  Volume: %d (Stream)\n", channel++, playing->m_audioEventRTS->getEventName().str(), filenameNoSlashes.str(), REAL_TO_INT(volume));
		}

		for (int i = channel; i <= channelCount; ++i)
		{
			fprintf(fp, "%2d: Silence\n", i);
		}
		fprintf(fp, "===========================================================\n");
	}
}
#endif

#ifdef AL_EXT_debug
// Debug callback for OpenAL errors - requires ALC_EXT_debug extension (OpenAL-soft >= 1.23 compiled with debug support)
static void AL_APIENTRY debugCallbackAL(ALenum source, ALenum type, ALuint id,
	ALenum severity, ALsizei length, const ALchar* message, void* userParam ) noexcept
{
	switch (severity)
	{
	case AL_DEBUG_SEVERITY_HIGH_EXT:
		DEBUG_LOG(("OpenAL Error: %s\n", message));
		break;
	case AL_DEBUG_SEVERITY_MEDIUM_EXT:
		DEBUG_LOG(("OpenAL Warning: %s\n", message));
		break;
	case AL_DEBUG_SEVERITY_LOW_EXT:
		DEBUG_LOG(("OpenAL Info: %s\n", message));
		break;
	default:
		DEBUG_LOG(("OpenAL Message: %s\n", message));
		break;
	}

}
#endif // AL_EXT_debug

ALenum OpenALAudioManager::getALFormat(uint8_t channels, uint8_t bitsPerSample)
{
	if (channels == 1 && bitsPerSample == 8)
		return AL_FORMAT_MONO8;
	if (channels == 1 && bitsPerSample == 16)
		return AL_FORMAT_MONO16;
	if (channels == 1 && bitsPerSample == 32)
		return AL_FORMAT_MONO_FLOAT32;
	if (channels == 2 && bitsPerSample == 8)
		return AL_FORMAT_STEREO8;
	if (channels == 2 && bitsPerSample == 16)
		return AL_FORMAT_STEREO16;
	if (channels == 2 && bitsPerSample == 32)
		return AL_FORMAT_STEREO_FLOAT32;

	DEBUG_LOG(("Unknown OpenAL format: %i channels, %i bits per sample", channels, bitsPerSample));
	return AL_FORMAT_MONO8;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::init()
{
	ScopedFPUGuard fpuGuard;
	AudioManager::init();
#ifdef INTENSE_DEBUG
	DEBUG_LOG(("Sound has temporarily been disabled in debug builds only. jkmcd\n"));
	// for now, _DEBUG builds only should have no sound. ask jkmcd or srj about this.
	return;
#endif

	// We should now know how many samples we want to load
	openDevice();
	m_audioCache->setMaxSize(getAudioSettings()->m_maxCacheSize);
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::postProcessLoad()
{
	AudioManager::postProcessLoad();
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::reset()
{
#if defined(_DEBUG) || defined(_INTERNAL)
	dumpAllAssetsUsed();
	m_allEventsLoaded.clear();
#endif

	AudioManager::reset();   // clears m_disallowSpeech
	s_disallowSpeechSetFrame = 0;  // GeneralsX @bugfix 14/06/2026 clear stale backstop frame across new game/map
	stopAllAudioImmediately();
	removeAllAudioRequests();
	// This must come after stopAllAudioImmediately() and removeAllAudioRequests(), to ensure that
	// sounds pointing to the temporary AudioEventInfo handles are deleted before their info is deleted
	removeLevelSpecificAudioEventInfos();
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::update()
{
	ScopedFPUGuard fpuGuard;
	AudioManager::update();
	setDeviceListenerPosition();
	processRequestList();
	processPlayingList();
	processFadingList();
	processStoppedList();
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::stopAudio(AudioAffect which)
{
	// All we really need to do is:
	// 1) Remove the EOS callback.
	// 2) Stop the sample, (so that when we later unload it, bad stuff doesn't happen)
	// 3) Set the status to stopped, so that when we next process the playing list, we will 
	//		correctly clean up the sample.


	std::list<PlayingAudio*>::iterator it;

	PlayingAudio* playing = NULL;
	if (BitIsSet(which, AudioAffect_Sound)) {
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			playing = *it;
			if (playing) {
				alSourceStop(playing->m_source);
			}
		}
	}

	if (BitIsSet(which, AudioAffect_Sound3D)) {
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			playing = *it;
			if (playing) {
				alSourceStop(playing->m_source);
			}
		}
	}

	if (BitIsSet(which, AudioAffect_Speech | AudioAffect_Music)) {
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
			playing = *it;
			// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
			if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getAudioEventInfo()) {
				if (playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music) {
					if (!BitIsSet(which, AudioAffect_Music)) {
						continue;
					}
				}
				else {
					if (!BitIsSet(which, AudioAffect_Speech)) {
						continue;
					}
				}
				// GeneralsX @bugfix BenderAI 11/03/2026 - streams use m_stream->stop(), not alSourceStop(m_source) which is always 0
				if (playing->m_stream) {
					playing->m_stream->stop();
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::pauseAudio(AudioAffect which)
{
	std::list<PlayingAudio*>::iterator it;

	PlayingAudio* playing = NULL;
	if (BitIsSet(which, AudioAffect_Sound)) {
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			playing = *it;
			if (playing) {
				alSourceStop(playing->m_source);
			}
		}
	}

	if (BitIsSet(which, AudioAffect_Sound3D)) {
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			playing = *it;
			if (playing) {
				alSourceStop(playing->m_source);
			}
		}
	}

	if (BitIsSet(which, AudioAffect_Speech | AudioAffect_Music)) {
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
			playing = *it;
			// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
			if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getAudioEventInfo()) {
				if (playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music) {
					if (!BitIsSet(which, AudioAffect_Music)) {
						continue;
					}
				}
				else {
					if (!BitIsSet(which, AudioAffect_Speech)) {
						continue;
					}
				}

				// GeneralsX @bugfix BenderAI 11/03/2026 - streams use m_stream->pause(), not alSourcePause(m_source) which is always 0
				if (playing->m_stream) {
					playing->m_stream->pause();
				}
			}
		}
	}

	//Get rid of PLAY audio requests when pausing audio.
	std::list<AudioRequest*>::iterator ait;
	for (ait = m_audioRequests.begin(); ait != m_audioRequests.end(); /* empty */)
	{
		AudioRequest* req = (*ait);
		if (req && req->m_request == AR_Play)
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
void OpenALAudioManager::resumeAudio(AudioAffect which)
{
	std::list<PlayingAudio*>::iterator it;

	PlayingAudio* playing = NULL;
	if (BitIsSet(which, AudioAffect_Sound)) {
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			playing = *it;
			if (playing) {
				alSourcePlay(playing->m_source);
			}
		}
	}

	if (BitIsSet(which, AudioAffect_Sound3D)) {
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			playing = *it;
			if (playing) {
				alSourcePlay(playing->m_source);
			}
		}
	}

	if (BitIsSet(which, AudioAffect_Speech | AudioAffect_Music)) {
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
			playing = *it;
			// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
			if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getAudioEventInfo()) {
				if (playing->m_audioEventRTS->getAudioEventInfo()->m_soundType == AT_Music) {
					if (!BitIsSet(which, AudioAffect_Music)) {
						continue;
					}
				}
				else {
					if (!BitIsSet(which, AudioAffect_Speech)) {
						continue;
					}
				}
				alSourcePlay(playing->m_stream->getSource());
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::pauseAmbient(Bool shouldPause)
{

}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::playAudioEvent(AudioEventRTS* event)
{
#ifdef INTENSIVE_AUDIO_DEBUG
	DEBUG_LOG(("OPENAL (%d) - Processing play request: %d (%s)", TheGameLogic->getFrame(), event->getPlayingHandle(), event->getEventName().str()));
#endif
	const AudioEventInfo* info = event->getAudioEventInfo();
	if (!info) {
		return;
	}

	std::list<PlayingAudio*>::iterator it;
	PlayingAudio* playing = NULL;

	AudioHandle handleToKill = event->getHandleToKill();

	AsciiString fileToPlay = event->getFilename();
	PlayingAudio* audio = allocatePlayingAudio();
	switch (info->m_soundType)
	{
	case AT_Music:
	case AT_Streaming:
	{
#ifdef INTENSIVE_AUDIO_DEBUG
		DEBUG_LOG(("- Stream\n"));
#endif

		if ((info->m_soundType == AT_Streaming) && event->getUninterruptible()) {
			stopAllSpeech();
		}

		Real curVolume = 1.0;
		if (info->m_soundType == AT_Music) {
			curVolume = m_musicVolume;
		}
		else {
			curVolume = m_speechVolume;
		}
		curVolume *= event->getVolume();

		Bool foundSoundToReplace = false;
		if (handleToKill) {
			for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
				playing = (*it);
				if (!playing) {
					continue;
				}

				if (playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == handleToKill)
				{
					//Release this streaming channel immediately because we are going to play another sound in it's place.
					releasePlayingAudio(playing);
					m_playingStreams.erase(it);
					foundSoundToReplace = true;
					break;
				}
			}
		}

		File* file = TheFileSystem->openFile(fileToPlay.str());
		if (!file) {
			DEBUG_LOG(("Failed to open file: %s\n", fileToPlay.str()));
			releasePlayingAudio(audio);
			return;
		}

		FFmpegFile* ffmpegFile = NEW FFmpegFile();
		if (!ffmpegFile->open(file))
		{
			DEBUG_LOG(("Failed to open FFmpeg file: %s\n", fileToPlay.str()));
			releasePlayingAudio(audio);
			return;
		}

		OpenALAudioStream* stream;
		if (!handleToKill || foundSoundToReplace) {
			stream = new OpenALAudioStream;
			// When we need more data ask FFmpeg for more data.
			stream->setRequireDataCallback([ffmpegFile, stream]() -> bool {
				ffmpegFile->decodePacket();
				// GeneralsX @bugfix 14/06/2026 Report TRUE end-of-file so a finished one-shot
				// speech (taunt) stops being restarted and can reach a stable AL_STOPPED. Keys
				// on real EOF, not a bare decode-error, so long briefings/music and transient
				// underruns are unaffected.
				return !ffmpegFile->isAtEof();
				});
			
			// When we receive a frame from FFmpeg, send it to OpenAL.
			ffmpegFile->setFrameCallback([stream](AVFrame* frame, int stream_idx, int stream_type, void* user_data) {
				if (stream_type != AVMEDIA_TYPE_AUDIO) {
					return;
				}

				DEBUG_LOG(("Received audio frame\n"));

				AVSampleFormat sampleFmt = static_cast<AVSampleFormat>(frame->format);
				const int bytesPerSample = av_get_bytes_per_sample(sampleFmt);
				ALenum format = OpenALAudioManager::getALFormat(frame->ch_layout.nb_channels, bytesPerSample * 8);
				const int frameSize =
					av_samples_get_buffer_size(NULL, frame->ch_layout.nb_channels, frame->nb_samples, sampleFmt, 1);
				uint8_t* frameData = frame->data[0];

				// We need to interleave the samples if the format is planar
				if (av_sample_fmt_is_planar(static_cast<AVSampleFormat>(frame->format))) {
					uint8_t* audioBuffer = static_cast<uint8_t*>(av_malloc(frameSize));

					// Write the samples into our audio buffer
					for (int sample_idx = 0; sample_idx < frame->nb_samples; sample_idx++)
					{
						int byte_offset = sample_idx * bytesPerSample;
						for (int channel_idx = 0; channel_idx < frame->ch_layout.nb_channels; channel_idx++)
						{
							uint8_t* dst = &audioBuffer[byte_offset * frame->ch_layout.nb_channels + channel_idx * bytesPerSample];
							uint8_t* src = &frame->data[channel_idx][byte_offset];
							memcpy(dst, src, bytesPerSample);
						}
					}
					stream->bufferData(audioBuffer, frameSize, format, frame->sample_rate);
					av_freep(&audioBuffer);
				}
				else
					stream->bufferData(frameData, frameSize, format, frame->sample_rate);
			});
		}
		else {
			stream = NULL;
		}

		// Put this on here, so that the audio event RTS will be cleaned up regardless.
		audio->m_audioEventRTS = event;
		audio->m_stream = stream;
		audio->m_ffmpegFile = ffmpegFile;
		audio->m_type = PAT_Stream;

		if (stream) {
			if ((info->m_soundType == AT_Streaming) && event->getUninterruptible()) {
				setDisallowSpeech(TRUE);
				s_disallowSpeechSetFrame = TheGameLogic ? TheGameLogic->getFrame() : 0;
			}
			// AIL_set_stream_volume_pan(stream, curVolume, 0.5f);
			playStream(event, stream);
			m_playingStreams.push_back(audio);
			audio = NULL;
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
					if (!playing) {
						continue;
					}

					if (playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == handleToKill)
					{
						//Release this 3D sound channel immediately because we are going to play another sound in it's place.
						releasePlayingAudio(playing);
						m_playing3DSounds.erase(it);
						foundSoundToReplace = true;
						break;
					}
				}
			}

			ALuint source;
			if (!handleToKill || foundSoundToReplace)
			{
				alGenSources(1, &source);

			}
			else
			{
				source = 0;
			}
			// Push it onto the list of playing things
			audio->m_audioEventRTS = event;
			audio->m_source = source;
			audio->m_bufferHandle = 0;
			audio->m_type = PAT_3DSample;
			m_playing3DSounds.push_back(audio);

			if (source) {
				audio->m_bufferHandle = playSample3D(event, audio);

			}

			if (!audio->m_bufferHandle)
			{
				m_playing3DSounds.pop_back();
#ifdef INTENSIVE_AUDIO_DEBUG
				DEBUG_LOG((" Killed (no handles available)\n"));
#endif
			}
			else
			{
				audio = NULL;
#ifdef INTENSIVE_AUDIO_DEBUG
				DEBUG_LOG((" Playing.\n"));
#endif
			}
		}
		else
		{
			// UI sounds are always 2-D. All other sounds should be Positional
			// Unit acknowledgement, etc, falls into the UI category of sound.
			Bool foundSoundToReplace = false;
			if (handleToKill) {
				for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
					playing = (*it);
					if (!playing) {
						continue;
					}

					if (playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == handleToKill)
					{
						//Release this 2D sound channel immediately because we are going to play another sound in it's place.
						releasePlayingAudio(playing);
						m_playingSounds.erase(it);
						foundSoundToReplace = true;
						break;
					}
				}
			}

			ALuint source;
			if (!handleToKill || foundSoundToReplace)
			{
				alGenSources(1, &source);
			}
			else
			{
				source = 0;
			}

			// Push it onto the list of playing things
			audio->m_audioEventRTS = event;
			audio->m_source = source;
			audio->m_bufferHandle = 0;
			audio->m_type = PAT_Sample;
			m_playingSounds.push_back(audio);

			if (source) {
				audio->m_bufferHandle = playSample(event, audio);

			}

			if (!audio->m_bufferHandle) {
#ifdef INTENSIVE_AUDIO_DEBUG
				DEBUG_LOG((" Killed (no handles available)\n"));
#endif
				m_playingSounds.pop_back();
			}
			else {
				audio = NULL;
			}

#ifdef INTENSIVE_AUDIO_DEBUG
			DEBUG_LOG((" Playing.\n"));
#endif
		}
		break;
	}
	}

	// If we were able to successfully play audio, then we set it to NULL above. (And it will be freed
	// later. However, if audio is non-NULL at this point, then it must be freed.
	if (audio) {
		releasePlayingAudio(audio);
	}
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::stopAudioEvent(AudioHandle handle)
{
#ifdef INTENSIVE_AUDIO_DEBUG
	DEBUG_LOG(("OPENAL (%d) - Processing stop request: %d\n", TheGameLogic->getFrame(), handle));
#endif

	std::list<PlayingAudio*>::iterator it;
	if (handle == AHSV_StopTheMusic || handle == AHSV_StopTheMusicFade) {
		// for music, just find the currently playing music stream and kill it.
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
			PlayingAudio* audio = (*it);
			if (!audio) {
				continue;
			}

			// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
			const AudioEventInfo* stopInfo = (audio->m_audioEventRTS ? audio->m_audioEventRTS->getAudioEventInfo() : nullptr);
			if (stopInfo && stopInfo->m_soundType == AT_Music)
			{
				if (handle == AHSV_StopTheMusicFade)
				{
					m_fadingAudio.push_back(audio);
				}
				else
				{
					//m_stoppedAudio.push_back(audio);
					releasePlayingAudio(audio);
				}
				m_playingStreams.erase(it);
				break;
			}
		}
	}

	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		PlayingAudio* audio = (*it);
		if (!audio || !audio->m_audioEventRTS) {
			continue;
		}

		if (audio->m_audioEventRTS->getPlayingHandle() == handle) {
			// found it
			audio->m_requestStop = true;
			notifyOfAudioCompletion((UnsignedInt)(audio->m_source), PAT_Stream);
			break;
		}
	}

	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		PlayingAudio* audio = (*it);
		if (!audio || !audio->m_audioEventRTS) {
			continue;
		}

		if (audio->m_audioEventRTS->getPlayingHandle() == handle) {
			audio->m_requestStop = true;
			break;
		}
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
		PlayingAudio* audio = (*it);
		if (!audio || !audio->m_audioEventRTS) {
			continue;
		}

		if (audio->m_audioEventRTS->getPlayingHandle() == handle) {
#ifdef INTENSIVE_AUDIO_DEBUG
			DEBUG_LOG((" (%s)\n", audio->m_audioEventRTS->getEventName()));
#endif
			audio->m_requestStop = true;
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::killAudioEventImmediately(AudioHandle audioEvent)
{
	//First look for it in the request list.
	std::list<AudioRequest*>::iterator ait;
	for (ait = m_audioRequests.begin(); ait != m_audioRequests.end(); ait++)
	{
		AudioRequest* req = (*ait);
		if (req && req->m_request == AR_Play && req->m_handleToInteractOn == audioEvent)
		{
			deleteInstance(req);
			ait = m_audioRequests.erase(ait);
			return;
		}
	}

	//Look for matching 3D sound to kill
	std::list<PlayingAudio*>::iterator it;
	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); it++)
	{
		PlayingAudio* audio = (*it);
		if (!audio)
		{
			continue;
		}

		if (audio->m_audioEventRTS && audio->m_audioEventRTS->getPlayingHandle() == audioEvent)
		{
			releasePlayingAudio(audio);
			m_playing3DSounds.erase(it);
			return;
		}
	}

	//Look for matching 2D sound to kill
	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); it++)
	{
		PlayingAudio* audio = (*it);
		if (!audio)
		{
			continue;
		}

		if (audio->m_audioEventRTS && audio->m_audioEventRTS->getPlayingHandle() == audioEvent)
		{
			releasePlayingAudio(audio);
			m_playingSounds.erase(it);
			return;
		}
	}

	//Look for matching steaming sound to kill
	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); it++)
	{
		PlayingAudio* audio = (*it);
		if (!audio)
		{
			continue;
		}

		if (audio->m_audioEventRTS && audio->m_audioEventRTS->getPlayingHandle() == audioEvent)
		{
			releasePlayingAudio(audio);
			m_playingStreams.erase(it);
			return;
		}
	}

}


//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::pauseAudioEvent(AudioHandle handle)
{
	// pause audio
}

//-------------------------------------------------------------------------------------------------
ALuint OpenALAudioManager::loadBufferForRead(AudioEventRTS* eventToLoadFrom)
{
	return m_audioCache->getBufferForFile(OpenFileInfo(eventToLoadFrom));
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::closeBuffer(ALuint bufferToClose)
{
	m_audioCache->closeBuffer(bufferToClose);
}


//-------------------------------------------------------------------------------------------------
PlayingAudio* OpenALAudioManager::allocatePlayingAudio(void)
{
	PlayingAudio* aud = NEW PlayingAudio;	// poolify
	return aud;
}


//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::releaseOpenALHandles(PlayingAudio* release)
{
	if (release->m_source)
	{
		alDeleteSources(1, &release->m_source);
		release->m_source = 0;
	}
	if (release->m_stream)
	{
		delete release->m_stream;
		release->m_stream = NULL;
	}
	if (release->m_ffmpegFile)
	{
		delete release->m_ffmpegFile;
		release->m_ffmpegFile = NULL;
	}

	release->m_type = PAT_INVALID;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::releasePlayingAudio(PlayingAudio* release)
{
	// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null getAudioEventInfo() return
	const AudioEventInfo* releaseInfo = (release->m_audioEventRTS ? release->m_audioEventRTS->getAudioEventInfo() : nullptr);
	if (releaseInfo && releaseInfo->m_soundType == AT_SoundEffect) {
		if (release->m_type == PAT_Sample) {
			if (release->m_source) {

			}
		}
		else {
			if (release->m_source) {

			}
		}
	}
	releaseOpenALHandles(release);	// forces stop of this audio
	closeBuffer(release->m_bufferHandle);
	if (release->m_cleanupAudioEventRTS) {
		releaseAudioEventRTS(release->m_audioEventRTS);
	}
	delete release;
	release = NULL;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::stopAllAudioImmediately(void)
{
	std::list<PlayingAudio*>::iterator it;
	PlayingAudio* playing;

	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ) {
		playing = *it;
		if (!playing) {
			continue;
		}

		releasePlayingAudio(playing);
		it = m_playingSounds.erase(it);
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ) {
		playing = *it;
		if (!playing) {
			continue;
		}

		releasePlayingAudio(playing);
		it = m_playing3DSounds.erase(it);
	}

	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ) {
		playing = (*it);
		if (!playing) {
			continue;
		}

		releasePlayingAudio(playing);
		it = m_playingStreams.erase(it);
	}

	for (it = m_fadingAudio.begin(); it != m_fadingAudio.end(); ) {
		playing = (*it);
		if (!playing) {
			continue;
		}

		releasePlayingAudio(playing);
		it = m_fadingAudio.erase(it);
	}

	//std::list<HAUDIO>::iterator hit;
	//for (hit = m_audioForcePlayed.begin(); hit != m_audioForcePlayed.end(); ++hit) {
	//	if (*hit) {
	//		AIL_quick_unload(*hit);
	//	}
	//}

	//m_audioForcePlayed.clear();
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::freeAllOpenALHandles(void)
{
	// First, we need to ensure that we don't have any sample handles open. To that end, we must stop
	// all of our currently playing audio.
	stopAllAudioImmediately();

	m_num2DSamples = 0;
	m_num3DSamples = 0;
	m_numStreams = 0;
}

//-------------------------------------------------------------------------------------------------
//HSAMPLE OpenALAudioManager::getFirst2DSample(AudioEventRTS* event)
//{
//	if (m_availableSamples.begin() != m_availableSamples.end()) {
//		HSAMPLE retSample = *m_availableSamples.begin();
//		m_availableSamples.erase(m_availableSamples.begin());
//		return (retSample);
//	}
//
//	// Find the first sample of lower priority than my augmented priority that is interruptable and take its handle
//
//	return NULL;
//}
//
////-------------------------------------------------------------------------------------------------
//PlayingAudio* OpenALAudioManager::getFirst3DSample(AudioEventRTS* event)
//{
//	if (m_available3DSamples.begin() != m_available3DSamples.end()) {
//		H3DSAMPLE retSample = *m_available3DSamples.begin();
//		m_available3DSamples.erase(m_available3DSamples.begin());
//		return (retSample);
//	}
//
//	// Find the first sample of lower priority than my augmented priority that is interruptable and take its handle
//	return NULL;
//}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::adjustPlayingVolume(PlayingAudio* audio)
{
	// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null m_audioEventRTS
	if (!audio->m_audioEventRTS)
		return;
	Real desiredVolume = audio->m_audioEventRTS->getVolume() * audio->m_audioEventRTS->getVolumeShift();
	if (audio->m_type == PAT_Sample) {
		alSourcef(audio->m_source, AL_GAIN, m_soundVolume * desiredVolume);

	}
	else if (audio->m_type == PAT_3DSample) {
		alSourcef(audio->m_source, AL_GAIN, m_sound3DVolume * desiredVolume);

	}
	else if (audio->m_type == PAT_Stream) {
		// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
		const AudioEventInfo* info = (audio->m_audioEventRTS ? audio->m_audioEventRTS->getAudioEventInfo() : nullptr);
		if (info && info->m_soundType == AT_Music) {
			alSourcef(audio->m_stream->getSource(), AL_GAIN, m_musicVolume * desiredVolume);
		}
		else {
			alSourcef(audio->m_stream->getSource(), AL_GAIN, m_speechVolume * desiredVolume);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::stopAllSpeech(void)
{
	std::list<PlayingAudio*>::iterator it;
	PlayingAudio* playing;
	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ) {
		playing = (*it);
		if (!playing) {
			++it;
			continue;
		}

		// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
		const AudioEventInfo* info = (playing->m_audioEventRTS ? playing->m_audioEventRTS->getAudioEventInfo() : nullptr);
		if (info && info->m_soundType == AT_Streaming) {
			releasePlayingAudio(playing);
			it = m_playingStreams.erase(it);
		}
		else {
			++it;
		}
	}

}

//-------------------------------------------------------------------------------------------------
//void OpenALAudioManager::initFilters(HSAMPLE sample, const AudioEventRTS* event)
//{
//	// set the sample volume
//	Real volume = event->getVolume() * event->getVolumeShift() * m_soundVolume;
//	AIL_set_sample_volume_pan(sample, volume, 0.5f);
//
//	// pitch shift
//	Real pitchShift = event->getPitchShift();
//	if (pitchShift == 0.0f) {
//		DEBUG_CRASH(("Invalid Pitch shift in sound: '%s'", event->getEventName().str()));
//	}
//	else {
//		AIL_set_sample_playback_rate(sample, REAL_TO_INT(AIL_sample_playback_rate(sample) * pitchShift));
//	}
//
//	// set up delay filter, if applicable
//	if (event->getDelay() > 0.0f) {
//		Real value;
//		value = event->getDelay();
//		AIL_set_sample_processor(sample, DP_FILTER, m_delayFilter);
//		AIL_set_filter_sample_preference(sample, "Mono Delay Time", &value);
//
//		value = 0.0;
//		AIL_set_filter_sample_preference(sample, "Mono Delay", &value);
//		AIL_set_filter_sample_preference(sample, "Mono Delay Mix", &value);
//	}
//}

//-------------------------------------------------------------------------------------------------
//void OpenALAudioManager::initFilters3D(H3DSAMPLE sample, const AudioEventRTS* event, const Coord3D* pos)
//{
//	// set the sample volume
//	Real volume = event->getVolume() * event->getVolumeShift() * m_sound3DVolume;
//	AIL_set_3D_sample_volume(sample, volume);
//
//	// pitch shift
//	Real pitchShift = event->getPitchShift();
//	if (pitchShift == 0.0f) {
//		DEBUG_CRASH(("Invalid Pitch shift in sound: '%s'", event->getEventName().str()));
//	}
//	else {
//		AIL_set_3D_sample_playback_rate(sample, REAL_TO_INT(AIL_3D_sample_playback_rate(sample) * pitchShift));
//	}
//
//	// Low pass filter
//	if (event->getAudioEventInfo()->m_lowPassFreq > 0 && !isOnScreen(pos)) {
//		AIL_set_3D_sample_occlusion(sample, 1.0f - event->getAudioEventInfo()->m_lowPassFreq);
//	}
//}

//-------------------------------------------------------------------------------------------------
AsciiString OpenALAudioManager::nextMusicTrack(void)
{
	AsciiString trackName;
	std::list<PlayingAudio*>::iterator it;
	PlayingAudio* playing;
	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = *it;
		// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
		const AudioEventInfo* info = (playing && playing->m_audioEventRTS) ? playing->m_audioEventRTS->getAudioEventInfo() : nullptr;
		if (info && info->m_soundType == AT_Music) {
			trackName = playing->m_audioEventRTS->getEventName();
		}
	}

	// Stop currently playing music
	TheAudio->removeAudioEvent(AHSV_StopTheMusic);

	trackName = nextTrackName(trackName);
	AudioEventRTS newTrack(trackName);
	TheAudio->addAudioEvent(&newTrack);

	return trackName;
}

//-------------------------------------------------------------------------------------------------
AsciiString OpenALAudioManager::prevMusicTrack(void)
{
	AsciiString trackName;
	std::list<PlayingAudio*>::iterator it;
	PlayingAudio* playing;
	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = *it;
		// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
		const AudioEventInfo* info = (playing && playing->m_audioEventRTS) ? playing->m_audioEventRTS->getAudioEventInfo() : nullptr;
		if (info && info->m_soundType == AT_Music) {
			trackName = playing->m_audioEventRTS->getEventName();
		}
	}

	// Stop currently playing music
	TheAudio->removeAudioEvent(AHSV_StopTheMusic);

	trackName = prevTrackName(trackName);
	AudioEventRTS newTrack(trackName);
	TheAudio->addAudioEvent(&newTrack);

	return trackName;
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::isMusicPlaying(void) const
{
	std::list<PlayingAudio*>::const_iterator it;
	PlayingAudio* playing;
	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = *it;
		// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
		const AudioEventInfo* info = (playing && playing->m_audioEventRTS) ? playing->m_audioEventRTS->getAudioEventInfo() : nullptr;
		if (info && info->m_soundType == AT_Music) {
			return TRUE;
		}
	}

	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::hasMusicTrackCompleted(const AsciiString& trackName, Int numberOfTimes) const
{
	std::list<PlayingAudio*>::const_iterator it;
	PlayingAudio* playing;
	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = *it;
		// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
		const AudioEventInfo* info = (playing && playing->m_audioEventRTS) ? playing->m_audioEventRTS->getAudioEventInfo() : nullptr;
		if (info && info->m_soundType == AT_Music) {
			if (playing->m_audioEventRTS->getEventName() == trackName) {
				//if (INFINITE_LOOP_COUNT - AIL_stream_loop_count(playing->m_stream) >= numberOfTimes) {
				// return TRUE;
				//}
			}
		}
	}

	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::openDevice(void)
{
	if (!TheGlobalData->m_audioOn) {
		return;
	}

	// AIL_quick_startup should be replaced later with a call to actually pick which device to use, etc
	const AudioSettings* audioSettings = getAudioSettings();
	m_selectedSpeakerType = TheAudio->translateSpeakerTypeToUnsignedInt(m_prefSpeaker);

	enumerateDevices();

	m_alcDevice = alcOpenDevice(NULL);
	if (m_alcDevice == nullptr) {
		DEBUG_LOG(("Failed to open ALC device"));
		// if we couldn't initialize any devices, turn sound off (fail silently)
		setOn(false, AudioAffect_All);
		return;
	}

	ALCint attributes[] = { ALC_FREQUENCY, audioSettings->m_outputRate, 0 /* end-of-list */ };
	m_alcContext = alcCreateContext(m_alcDevice, attributes);
	if (m_alcContext == nullptr) {
		DEBUG_LOG(("Failed to create ALC context"));
		setOn(false, AudioAffect_All);
		return;
	}

	if (!alcMakeContextCurrent(m_alcContext)) {
		DEBUG_LOG(("Failed to make ALC context current"));
		setOn(false, AudioAffect_All);
		return;
	}

#ifdef AL_EXT_debug
	if (alcIsExtensionPresent(m_alcDevice, "ALC_EXT_debug")) {
		auto alDebugMessageCallbackEXT = LPALDEBUGMESSAGECALLBACKEXT{};
		LOAD_ALC_PROC(alDebugMessageCallbackEXT);
		alEnable(AL_DEBUG_OUTPUT_EXT);
		alDebugMessageCallbackEXT(debugCallbackAL, nullptr);
	}
#endif // AL_EXT_debug

	selectProvider(TheAudio->getProviderIndex(m_pref3DProvider));

	// Now that we're all done, update the cached variables so that everything is in sync.
	TheAudio->refreshCachedVariables();

	if (!isValidProvider()) {
		return;
	}

	initDelayFilter();
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::closeDevice(void)
{
	unselectProvider();
	alcMakeContextCurrent(nullptr);

	if (m_alcContext)
		alcDestroyContext(m_alcContext);

	if (m_alcDevice)
		alcCloseDevice(m_alcDevice);
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::isCurrentlyPlaying(AudioHandle handle)
{
	std::list<PlayingAudio*>::iterator it;
	PlayingAudio* playing;

	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == handle) {
			return true;
		}
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == handle) {
			return true;
		}
	}

	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getPlayingHandle() == handle) {
			return true;
		}
	}

	// if something is requested, it is also considered playing
	std::list<AudioRequest*>::iterator ait;
	AudioRequest* req = NULL;
	for (ait = m_audioRequests.begin(); ait != m_audioRequests.end(); ++ait) {
		req = *ait;
		if (req && req->m_usePendingEvent && req->m_pendingEvent->getPlayingHandle() == handle) {
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::notifyOfAudioCompletion(UnsignedInt audioCompleted, UnsignedInt flags)
{
	PlayingAudio* playing = findPlayingAudioFrom(audioCompleted, flags);
	if (!playing) {
		DEBUG_CRASH(("Audio has completed playing, but we can't seem to find it. - jkmcd"));
		return;
	}

	// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
	if (!playing->m_audioEventRTS)
		return;
	const AudioEventInfo* completionInfo = playing->m_audioEventRTS->getAudioEventInfo();
	if (!completionInfo)
		return;

	if (getDisallowSpeech() && completionInfo->m_soundType == AT_Streaming) {
		setDisallowSpeech(FALSE);
	}

	if (completionInfo->m_control & AC_LOOP) {
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
			closeBuffer(playing->m_bufferHandle);	// close it so as not to leak it.
			playing->m_bufferHandle = playSample(playing->m_audioEventRTS, playing);

			// If we don't have a file now, then we should drop to the stopped status so that 
			// We correctly close this handle.
			if (playing->m_bufferHandle) {
				return;
			}
		}
		else if (playing->m_type == PAT_3DSample) {
			closeBuffer(playing->m_bufferHandle);	// close it so as not to leak it.
			playing->m_bufferHandle = playSample3D(playing->m_audioEventRTS, playing);

			// If we don't have a file now, then we should drop to the stopped status so that 
			// We correctly close this handle.
			if (playing->m_bufferHandle) {
				return;
			}
		}
	}

	if (playing->m_type == PAT_Stream) {
		// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
		const AudioEventInfo* info = (playing->m_audioEventRTS ? playing->m_audioEventRTS->getAudioEventInfo() : nullptr);
		if (info && info->m_soundType == AT_Music) {
			playStream(playing->m_audioEventRTS, playing->m_stream);

			return;
		}
	}
}

//-------------------------------------------------------------------------------------------------
PlayingAudio* OpenALAudioManager::findPlayingAudioFrom(ALuint source, UnsignedInt flags)
{
	std::list<PlayingAudio*>::iterator it;
	PlayingAudio* playing;

	if (flags == PAT_Sample) {
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			playing = *it;
			if (playing && playing->m_source == source) {
				return playing;
			}
		}
	}

	if (flags == PAT_3DSample) {
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			playing = *it;
			if (playing && playing->m_source == source) {
				return playing;
			}
		}
	}

	if (flags == PAT_Stream) {
		for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
			playing = *it;
			if (playing && playing->m_source == source) {
				return playing;
			}
		}
	}

	return NULL;
}


//-------------------------------------------------------------------------------------------------
UnsignedInt OpenALAudioManager::getProviderCount(void) const
{
	return m_providerCount;
}

//-------------------------------------------------------------------------------------------------
AsciiString OpenALAudioManager::getProviderName(UnsignedInt providerNum) const
{
	if (isOn(AudioAffect_Sound3D) && providerNum < m_providerCount) {
		return m_provider3D[providerNum].name;
	}

	return AsciiString::TheEmptyString;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt OpenALAudioManager::getProviderIndex(AsciiString providerName) const
{
	for (UnsignedInt i = 0; i < m_providerCount; ++i) {
		if (providerName == m_provider3D[i].name) {
			return i;
		}
	}

	return PROVIDER_ERROR;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::selectProvider(UnsignedInt providerNdx)
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
		freeAllOpenALHandles();
		unselectProvider();
	}

	/*LPDIRECTSOUND lpDirectSoundInfo;
	AIL_get_DirectSound_info(NULL, (void**)&lpDirectSoundInfo, NULL);
	Bool useDolby = FALSE;
	if (lpDirectSoundInfo)
	{
		DWORD speakerConfig;
		lpDirectSoundInfo->GetSpeakerConfig(&speakerConfig);
		switch (DSSPEAKER_CONFIG(speakerConfig))
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

	if (useDolby)
	{
		providerNdx = getProviderIndex("Dolby Surround");
	}
	else
	{
		providerNdx = getProviderIndex("Miles Fast 2D Positional Audio");
	}
	success = AIL_open_3D_provider(m_provider3D[providerNdx].id) == 0;*/

	//if (providerNdx < m_providerCount) 
	//{
	//	failed = AIL_open_3D_provider(m_provider3D[providerNdx].id);
	//}

	Bool success = FALSE;

	if (!success)
	{
		m_selectedProvider = PROVIDER_ERROR;
		// try to select a failsafe
		providerNdx = getProviderIndex("Miles Fast 2D Positional Audio");
		success = TRUE;
	}

	if (success)
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
void OpenALAudioManager::unselectProvider(void)
{
	if (!(isOn(AudioAffect_Sound3D) && isValidProvider())) {
		return;
	}

	if (TheVideoPlayer) {
		TheVideoPlayer->notifyVideoPlayerOfNewProvider(FALSE);
	}
	//AIL_close_3D_listener(m_listener);
	//m_listener = NULL;

	//AIL_close_3D_provider(m_provider3D[m_selectedProvider].id);
	m_lastProvider = m_selectedProvider;

	m_selectedProvider = PROVIDER_ERROR;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt OpenALAudioManager::getSelectedProvider(void) const
{
	return m_selectedProvider;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::setSpeakerType(UnsignedInt speakerType)
{
	if (!isValidProvider()) {
		return;
	}

	//AIL_set_3D_speaker_type(m_provider3D[m_selectedProvider].id, speakerType);
	m_selectedSpeakerType = speakerType;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt OpenALAudioManager::getSpeakerType(void)
{
	if (!isValidProvider()) {
		return 0;
	}

	return m_selectedSpeakerType;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt OpenALAudioManager::getNum2DSamples(void) const
{
	return m_num2DSamples;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt OpenALAudioManager::getNum3DSamples(void) const
{
	return m_num3DSamples;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt OpenALAudioManager::getNumStreams(void) const
{
	return m_numStreams;
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::doesViolateLimit(AudioEventRTS* event) const
{
	Int limit = event->getAudioEventInfo()->m_limit;
	if (limit == 0) {
		return false;
	}

	Int totalCount = 0;
	Int totalRequestCount = 0;

	std::list<PlayingAudio*>::const_iterator it;
	if (!event->isPositionalAudio()) {
		// 2-D
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			if (!(*it)->m_audioEventRTS) continue; // GeneralsX @bugfix BenderAI 11/03/2026
			if ((*it)->m_audioEventRTS->getEventName() == event->getEventName()) {
				if (totalCount == 0) {
					// This is the oldest audio of this type playing.
					event->setHandleToKill((*it)->m_audioEventRTS->getPlayingHandle());
				}
				++totalCount;
			}
		}
	}
	else {
		// 3-D
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			if (!(*it)->m_audioEventRTS) continue; // GeneralsX @bugfix BenderAI 11/03/2026
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
		AudioRequest* req = (*arIt);
		if (req == NULL) {
			continue;
		}
		if (req->m_usePendingEvent)
		{
			if (req->m_pendingEvent->getEventName() == event->getEventName())
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
	if (event->getAudioEventInfo()->m_control & AC_INTERRUPT)
	{
		if (totalRequestCount < limit)
		{
			Int totalPlayingCount = totalCount - totalRequestCount;
			if (totalRequestCount + totalPlayingCount < limit)
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

	if (totalCount < limit)
	{
		event->setHandleToKill(0);
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::isPlayingAlready(AudioEventRTS* event) const
{
	std::list<PlayingAudio*>::const_iterator it;
	if (!event->isPositionalAudio()) {
		// 2-D
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			if (!(*it)->m_audioEventRTS) continue; // GeneralsX @bugfix BenderAI 11/03/2026
			if ((*it)->m_audioEventRTS->getEventName() == event->getEventName()) {
				return true;
			}
		}
	}
	else {
		// 3-D
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			if (!(*it)->m_audioEventRTS) continue; // GeneralsX @bugfix BenderAI 11/03/2026
			if ((*it)->m_audioEventRTS->getEventName() == event->getEventName()) {
				return true;
			}
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::isObjectPlayingVoice(UnsignedInt objID) const
{
	if (objID == 0) {
		return false;
	}

	std::list<PlayingAudio*>::const_iterator it;
	// 2-D
	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		if (!(*it)->m_audioEventRTS) continue; // GeneralsX @bugfix BenderAI 11/03/2026
		const AudioEventInfo* info = (*it)->m_audioEventRTS->getAudioEventInfo();
		if (info && (*it)->m_audioEventRTS->getObjectID() == objID && (info->m_type & ST_VOICE)) {
			return true;
		}
	}

	// 3-D
	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
		if (!(*it)->m_audioEventRTS) continue; // GeneralsX @bugfix BenderAI 11/03/2026
		const AudioEventInfo* info = (*it)->m_audioEventRTS->getAudioEventInfo();
		if (info && (*it)->m_audioEventRTS->getObjectID() == objID && (info->m_type & ST_VOICE)) {
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
AudioEventRTS* OpenALAudioManager::findLowestPrioritySound(AudioEventRTS* event)
{
	AudioPriority priority = event->getAudioEventInfo()->m_priority;
	if (priority == AP_LOWEST)
	{
		//If the event we pass in is the lowest priority, don't bother checking because
		//there is nothing lower priority than lowest.
		return NULL;
	}
	AudioEventRTS* lowestPriorityEvent = NULL;
	AudioPriority lowestPriority;

	std::list<PlayingAudio*>::const_iterator it;
	if (event->isPositionalAudio())
	{
		//3D
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it)
		{
			AudioEventRTS* itEvent = (*it)->m_audioEventRTS;
			if (!itEvent) continue; // GeneralsX @bugfix BenderAI 11/03/2026
			const AudioEventInfo* itInfo = itEvent->getAudioEventInfo();
			if (!itInfo) continue;
			AudioPriority itPriority = itInfo->m_priority;
			if (itPriority < priority)
			{
				if (!lowestPriorityEvent || lowestPriority > itPriority)
				{
					lowestPriorityEvent = itEvent;
					lowestPriority = itPriority;
					if (lowestPriority == AP_LOWEST)
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
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it)
		{
			AudioEventRTS* itEvent = (*it)->m_audioEventRTS;
			if (!itEvent) continue; // GeneralsX @bugfix BenderAI 11/03/2026
			const AudioEventInfo* itInfo = itEvent->getAudioEventInfo();
			if (!itInfo) continue;
			AudioPriority itPriority = itInfo->m_priority;
			if (itPriority < priority)
			{
				if (!lowestPriorityEvent || lowestPriority > itPriority)
				{
					lowestPriorityEvent = itEvent;
					lowestPriority = itPriority;
					if (lowestPriority == AP_LOWEST)
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
Bool OpenALAudioManager::isPlayingLowerPriority(AudioEventRTS* event) const
{
	//We don't actually want to do anything to this CONST function. Remember, we're
	//just checking to see if there is a lower priority sound.
	AudioPriority priority = event->getAudioEventInfo()->m_priority;
	if (priority == AP_LOWEST)
	{
		//If the event we pass in is the lowest priority, don't bother checking because
		//there is nothing lower priority than lowest.
		return false;
	}
	std::list<PlayingAudio*>::const_iterator it;
	if (!event->isPositionalAudio()) {
		// 2-D
		for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
			if (!(*it)->m_audioEventRTS) continue; // GeneralsX @bugfix BenderAI 11/03/2026
			const AudioEventInfo* info = (*it)->m_audioEventRTS->getAudioEventInfo();
			if (info && info->m_priority < priority) {
				event->setHandleToKill((*it)->m_audioEventRTS->getPlayingHandle());
				return true;
			}
		}
	}
	else {
		// 3-D
		for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
			if (!(*it)->m_audioEventRTS) continue; // GeneralsX @bugfix BenderAI 11/03/2026
			const AudioEventInfo* info = (*it)->m_audioEventRTS->getAudioEventInfo();
			if (info && info->m_priority < priority) {
				event->setHandleToKill((*it)->m_audioEventRTS->getPlayingHandle());
				return true;
			}
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::killLowestPrioritySoundImmediately(AudioEventRTS* event)
{
	//Actually, we want to kill the LOWEST PRIORITY SOUND, not the first "lower" priority
	//sound we find, because it could easily be 
	AudioEventRTS* lowestPriorityEvent = findLowestPrioritySound(event);
	if (lowestPriorityEvent)
	{
		std::list<PlayingAudio*>::iterator it;
		if (event->isPositionalAudio())
		{
			for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it)
			{
				PlayingAudio* playing = (*it);
				if (!playing)
				{
					continue;
				}

				if (playing->m_audioEventRTS && playing->m_audioEventRTS == lowestPriorityEvent)
				{
					//Release this 3D sound channel immediately because we are going to play another sound in it's place.
					releasePlayingAudio(playing);
					m_playing3DSounds.erase(it);
					return TRUE;
				}
			}
		}
		else
		{
			for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it)
			{
				PlayingAudio* playing = (*it);
				if (!playing)
				{
					continue;
				}

				if (playing->m_audioEventRTS && playing->m_audioEventRTS == lowestPriorityEvent)
				{
					//Release this 3D sound channel immediately because we are going to play another sound in it's place.
					releasePlayingAudio(playing);
					m_playing3DSounds.erase(it);
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}


//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::adjustVolumeOfPlayingAudio(AsciiString eventName, Real newVolume)
{
	std::list<PlayingAudio*>::iterator it;

	PlayingAudio* playing = NULL;
	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ++it) {
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getEventName() == eventName) {
			// Adjust it
			playing->m_audioEventRTS->setVolume(newVolume);
			Real desiredVolume = playing->m_audioEventRTS->getVolume() * playing->m_audioEventRTS->getVolumeShift();
			alSourcef(playing->m_source, AL_GAIN, desiredVolume);
		}
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ++it) {
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getEventName() == eventName) {
			// Adjust it
			playing->m_audioEventRTS->setVolume(newVolume);
			Real desiredVolume = playing->m_audioEventRTS->getVolume() * playing->m_audioEventRTS->getVolumeShift();
			alSourcef(playing->m_source, AL_GAIN, desiredVolume);
		}
	}

	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it) {
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getEventName() == eventName) {
			// Adjust it
			playing->m_audioEventRTS->setVolume(newVolume);
			Real desiredVolume = playing->m_audioEventRTS->getVolume() * playing->m_audioEventRTS->getVolumeShift();
			alSourcef(playing->m_source, AL_GAIN, desiredVolume);
		}
	}
}


//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::removePlayingAudio(AsciiString eventName)
{
	std::list<PlayingAudio*>::iterator it;

	PlayingAudio* playing = NULL;
	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); )
	{
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getEventName() == eventName)
		{
			releasePlayingAudio(playing);
			it = m_playingSounds.erase(it);
		}
		else
		{
			it++;
		}
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); )
	{
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getEventName() == eventName)
		{
			releasePlayingAudio(playing);
			it = m_playing3DSounds.erase(it);
		}
		else
		{
			it++;
		}
	}

	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); )
	{
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getEventName() == eventName)
		{
			releasePlayingAudio(playing);
			it = m_playingStreams.erase(it);
		}
		else
		{
			it++;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::removeAllDisabledAudio()
{
	std::list<PlayingAudio*>::iterator it;

	PlayingAudio* playing = NULL;
	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); )
	{
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getVolume() == 0.0f)
		{
			releasePlayingAudio(playing);
			it = m_playingSounds.erase(it);
		}
		else
		{
			it++;
		}
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); )
	{
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getVolume() == 0.0f)
		{
			releasePlayingAudio(playing);
			it = m_playing3DSounds.erase(it);
		}
		else
		{
			it++;
		}
	}

	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); )
	{
		playing = *it;
		if (playing && playing->m_audioEventRTS && playing->m_audioEventRTS->getVolume() == 0.0f)
		{
			releasePlayingAudio(playing);
			it = m_playingStreams.erase(it);
		}
		else
		{
			it++;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::processRequestList(void)
{
	std::list<AudioRequest*>::iterator it;
	for (it = m_audioRequests.begin(); it != m_audioRequests.end(); /* empty */) {
		AudioRequest* req = (*it);
		if (req == NULL) {
			continue;
		}

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
void OpenALAudioManager::processPlayingList(void)
{
	// There are two types of processing we have to do here. 
	// 1. Move the item to the stopped list if it has become stopped.
	// 2. Update the position of the audio if it is positional
	std::list<PlayingAudio*>::iterator it;
	PlayingAudio* playing;

	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); /* empty */) {
		playing = (*it);
		if (!playing)
		{
			it = m_playingSounds.erase(it);
			continue;
		}

		if (sourceIsStopped(playing->m_source))
		{
			// GeneralsX @bugfix BenderAI 09/05/2026 - Advance through Attack/Sound/Decay portions.
			// Miles used EOS callbacks; OpenAL requires polling. Without this call the Attack
			// (static/intro) portion plays but Sound (voice) is never started.
			if (!playing->m_requestStop)
				notifyOfAudioCompletion(playing->m_source, PAT_Sample);
			// If notifyOfAudioCompletion started the next portion the source is now playing;
			// only erase when it is still stopped (done or failed to start next portion).
			if (sourceIsStopped(playing->m_source))
			{
				//m_stoppedAudio.push_back(playing);
				releasePlayingAudio(playing);
				it = m_playingSounds.erase(it);
			}
			else
			{
				++it;
			}
		}
		else
		{
			if (m_volumeHasChanged)
			{
				adjustPlayingVolume(playing);
			}
			++it;
		}
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); )
	{
		playing = (*it);
		if (!playing)
		{
			it = m_playing3DSounds.erase(it);
			continue;
		}

		if (sourceIsStopped(playing->m_source))
		{
			// GeneralsX @bugfix BenderAI 09/05/2026 - Same fix as for 2D samples: advance
			// Attack→Sound→Decay before releasing. Miles used EOS callbacks; we must poll.
			if (!playing->m_requestStop)
				notifyOfAudioCompletion(playing->m_source, PAT_3DSample);
			if (sourceIsStopped(playing->m_source))
			{
				//m_stoppedAudio.push_back(playing);
				releasePlayingAudio(playing);
				it = m_playing3DSounds.erase(it);
			}
			else
			{
				++it;
			}
		}
		else
		{
			if (m_volumeHasChanged)
			{
				adjustPlayingVolume(playing);
			}

			const Coord3D* pos = getCurrentPositionFromEvent(playing->m_audioEventRTS);
			if (pos)
			{
				if (playing->m_audioEventRTS->isDead())
				{
					stopAudioEvent(playing->m_audioEventRTS->getPlayingHandle());
					it++;
					continue;
				}
				else
				{
					Real volForConsideration = getEffectiveVolume(playing->m_audioEventRTS);
					volForConsideration /= (m_sound3DVolume > 0.0f ? m_soundVolume : 1.0f);
					// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null getAudioEventInfo()
				const AudioEventInfo* pai = (playing->m_audioEventRTS ? playing->m_audioEventRTS->getAudioEventInfo() : nullptr);
				Bool playAnyways = pai && (BitIsSet(pai->m_type, ST_GLOBAL) || pai->m_priority == AP_CRITICAL);
					if (volForConsideration < m_audioSettings->m_minVolume && !playAnyways)
					{
						// don't want to get an additional callback for this sample
						//AIL_register_3D_EOS_callback(playing->m_3DSample, NULL);
						//m_stoppedAudio.push_back(playing);
						releasePlayingAudio(playing);
						it = m_playing3DSounds.erase(it);
						continue;
					}
					else
					{
						Real x = pos->x;
						Real y = pos->y;
						Real z = pos->z;
						alSource3f(playing->m_source, AL_POSITION, x, y, z);
						// DEBUG_LOG(("Updating 3D sound position for %s to %f, %f, %f\n", playing->m_audioEventRTS->getEventName().str(), x, y, z));
					}
				}
			}
			else
			{
				//AIL_register_3D_EOS_callback(playing->m_3DSample, NULL);
				//m_stoppedAudio.push_back(playing);
				releasePlayingAudio(playing);
				it = m_playing3DSounds.erase(it);
				continue;
			}

			++it;
		}
	}

	for (it = m_playingStreams.begin(); it != m_playingStreams.end(); ) {
		playing = (*it);
		if (!playing)
		{
			it = m_playingStreams.erase(it);
			continue;
		}

		// GeneralsX @bugfix BenderAI 11/03/2026 - streams use m_stream->getSource(), not m_source (which is always 0).
		// Call update() first so buffer-underrun recovery (play()) runs before we check stopped state.
		if (m_volumeHasChanged)
		{
			adjustPlayingVolume(playing);
		}
		playing->m_stream->update();

		// After update(), if stream source is still stopped there is no more data — release it.
		if (playing->m_stream && sourceIsStopped(playing->m_stream->getSource()))
		{
			// If this was an uninterruptible speech stream, clear the disallow flag.
			const AudioEventInfo* streamInfo = (playing->m_audioEventRTS ? playing->m_audioEventRTS->getAudioEventInfo() : nullptr);
			if (streamInfo && streamInfo->m_soundType == AT_Streaming && getDisallowSpeech())
			{
				setDisallowSpeech(FALSE);
			}
			//m_stoppedAudio.push_back(playing);
			releasePlayingAudio(playing);
			it = m_playingStreams.erase(it);
		}
		else
		{
			++it;
		}
	}

	// GeneralsX @bugfix 14/06/2026 Backstop (belt-and-braces): the proper fix is the EOF
	// propagation in OpenALAudioStream (a finished one-shot now reaches AL_STOPPED so the
	// per-stream clear above fires the frame the audio ends). This timeout remains only as a
	// safety net in case AL_STOPPED detection ever fails on this iOS/OpenAL stack. A negative
	// delta means the logic frame counter reset under us (new game/map) — treat the recorded
	// frame as stale and clear too. Never cuts a genuinely-playing line (longer than any voice).
	if (getDisallowSpeech() && TheGameLogic) {
		const Int sinceSet = (Int)TheGameLogic->getFrame() - s_disallowSpeechSetFrame;
		if (sinceSet < 0 || sinceSet > DISALLOW_SPEECH_MAX_FRAMES) {
			setDisallowSpeech(FALSE);
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

Bool OpenALAudioManager::has3DSensitiveStreamsPlaying(void) const
{
	if (m_playingStreams.empty())
		return FALSE;

	for (std::list< PlayingAudio* >::const_iterator it = m_playingStreams.begin(); it != m_playingStreams.end(); ++it)
	{
		const PlayingAudio* playing = (*it);

		if (!playing)
			continue;

		// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null audioEventRTS/info
		if (!playing->m_audioEventRTS)
			continue;

		const AudioEventInfo* info = playing->m_audioEventRTS->getAudioEventInfo();
		if (!info)
			continue;

		if (info->m_soundType != AT_Music)
		{
			return TRUE;
		}

		if (playing->m_audioEventRTS->getEventName().startsWith("Game_") == FALSE)
		{
			return TRUE;
		}
	}

	return FALSE;

}


//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::processFadingList(void)
{
	std::list<PlayingAudio*>::iterator it;
	PlayingAudio* playing;

	for (it = m_fadingAudio.begin(); it != m_fadingAudio.end(); /* emtpy */) {
		playing = *it;
		if (!playing) {
			continue;
		}

		if (playing->m_framesFaded >= getAudioSettings()->m_fadeAudioFrames) {
			playing->m_requestStop = true;
			//m_stoppedAudio.push_back(playing);
			releasePlayingAudio(playing);
			it = m_fadingAudio.erase(it);
			continue;
		}

		++playing->m_framesFaded;
		Real volume = getEffectiveVolume(playing->m_audioEventRTS);
		volume *= (1.0f - 1.0f * playing->m_framesFaded / getAudioSettings()->m_fadeAudioFrames);

		switch (playing->m_type)
		{
		case PAT_Sample:
		{
			alSourcef(playing->m_source, AL_GAIN, volume);
			break;
		}

		case PAT_3DSample:
		{
			alSourcef(playing->m_source, AL_GAIN, volume);
			break;
		}

		case PAT_Stream:
		{
			alSourcef(playing->m_stream->getSource(), AL_GAIN, volume);
			break;
		}

		}

		++it;
	}
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::processStoppedList(void)
{
	std::list<PlayingAudio*>::iterator it;
	PlayingAudio* playing;

	for (it = m_stoppedAudio.begin(); it != m_stoppedAudio.end(); /* emtpy */) {
		playing = *it;
		if (playing) {
			releasePlayingAudio(playing);
		}
		it = m_stoppedAudio.erase(it);
	}
}


//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::shouldProcessRequestThisFrame(AudioRequest* req) const
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
void OpenALAudioManager::adjustRequest(AudioRequest* req)
{
	if (!req->m_usePendingEvent) {
		return;
	}

	req->m_pendingEvent->decrementDelay(MSEC_PER_LOGICFRAME_REAL);
	req->m_requiresCheckForSample = true;
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::checkForSample(AudioRequest* req)
{
	if (!req->m_usePendingEvent) {
		return true;
	}

	if (req->m_pendingEvent->getAudioEventInfo() == NULL)
	{
		// Fill in event info
		getInfoForAudioEvent(req->m_pendingEvent);
	}

	if (req->m_pendingEvent->getAudioEventInfo()->m_type != AT_SoundEffect)
	{
		return true;
	}

	return m_sound->canPlayNow(req->m_pendingEvent);
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::setHardwareAccelerated(Bool accel)
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
void OpenALAudioManager::setSpeakerSurround(Bool surround)
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
	}
	else {
		speakerType = TheAudio->getAudioSettings()->m_defaultSpeakerType2D;
	}

	TheAudio->setSpeakerType(speakerType);
}

//-------------------------------------------------------------------------------------------------
Real OpenALAudioManager::getFileLengthMS(AsciiString strToLoad) const
{
	if (strToLoad.isEmpty()) {
		return 0.0f;
	}
	float length = 0.0f;

#ifdef SAGE_USE_FFMPEG
	ALuint handle = m_audioCache->getBufferForFile(OpenFileInfo(&strToLoad));
	length = m_audioCache->getBufferLength(handle);
	m_audioCache->closeBuffer(handle);
#endif

	return length;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::closeAnySamplesUsingFile(const void* fileToClose)
{
	ALuint bufferHandle = (ALuint)(uintptr_t)fileToClose;
	if (!bufferHandle) {
		return;
	}

	std::list<PlayingAudio*>::iterator it;
	PlayingAudio* playing;

	for (it = m_playingSounds.begin(); it != m_playingSounds.end(); ) {
		playing = *it;
		if (!playing) {
			continue;
		}

		if (playing->m_bufferHandle == bufferHandle) {
			releasePlayingAudio(playing);
			it = m_playingSounds.erase(it);
		}
		else {
			++it;
		}
	}

	for (it = m_playing3DSounds.begin(); it != m_playing3DSounds.end(); ) {
		playing = *it;
		if (!playing) {
			continue;
		}

		if (playing->m_bufferHandle == bufferHandle) {
			releasePlayingAudio(playing);
			it = m_playing3DSounds.erase(it);
		}
		else {
			++it;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::setDeviceListenerPosition(void)
{
	ALfloat listenerOri[] = { m_listenerOrientation.x, m_listenerOrientation.y, m_listenerOrientation.z, 0.0f, 0.0f, 1.0f };
	alListener3f(AL_POSITION, m_listenerPosition.x, m_listenerPosition.y, m_listenerPosition.z);
	alListenerfv(AL_ORIENTATION, listenerOri);
	DEBUG_LOG(("Listener Position: %f, %f, %f\n", m_listenerPosition.x, m_listenerPosition.y, m_listenerPosition.z));
}

//-------------------------------------------------------------------------------------------------
const Coord3D* OpenALAudioManager::getCurrentPositionFromEvent(AudioEventRTS* event)
{
	if (!event->isPositionalAudio()) {
		return NULL;
	}

	return event->getCurrentPosition();
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::isOnScreen(const Coord3D* pos) const
{
	static ICoord2D dummy;
	// WorldToScreen will return True if the point is onscreen and false if it is offscreen.
	return TheTacticalView->worldToScreen(pos, &dummy);
}

//-------------------------------------------------------------------------------------------------
Real OpenALAudioManager::getEffectiveVolume(AudioEventRTS* event) const
{
	// GeneralsX @bugfix BenderAI 11/03/2026 - guard against null event or getAudioEventInfo()
	if (!event)
		return 0.0f;
	Real volume = 1.0f;
	volume *= (event->getVolume() * event->getVolumeShift());
	const AudioEventInfo* evInfo = event->getAudioEventInfo();
	if (evInfo && evInfo->m_soundType == AT_Music)
	{
		volume *= m_musicVolume;
	}
	else if (evInfo && evInfo->m_soundType == AT_Streaming)
	{
		volume *= m_speechVolume;
	}
	else
	{
		if (event->isPositionalAudio())
		{
			volume *= m_sound3DVolume;
		}
		else
		{
			volume *= m_soundVolume;
		}
	}

	return volume;
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::startNextLoop(PlayingAudio* looping)
{
	closeBuffer(looping->m_bufferHandle);
	looping->m_bufferHandle = 0;

	if (looping->m_requestStop) {
		return false;
	}

	if (looping->m_audioEventRTS->hasMoreLoops()) {
		// generate a new filename, and test to see whether we can play with it now
		looping->m_audioEventRTS->generateFilename();

		if (looping->m_audioEventRTS->getDelay() > MSEC_PER_LOGICFRAME_REAL) {
			// fake it out so that this sound appears done, but also so that it will not
			// delete the sound on completion (which would suck)
			looping->m_cleanupAudioEventRTS = false;
			looping->m_requestStop = true;

			AudioRequest* req = allocateAudioRequest(true);
			req->m_pendingEvent = looping->m_audioEventRTS;
			req->m_requiresCheckForSample = true;
			appendAudioRequest(req);
			return true;
		}

		if (looping->m_type == PAT_3DSample) {
			looping->m_bufferHandle = playSample3D(looping->m_audioEventRTS, looping);
		}
		else {
			looping->m_bufferHandle = playSample(looping->m_audioEventRTS, looping);
		}

		return looping->m_bufferHandle != 0;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::playStream(AudioEventRTS* event, OpenALAudioStream* stream)
{
	// Force it to the beginning
	if (event->getAudioEventInfo()->m_soundType == AT_Music) {
		//alSourcei(stream->getSource(), AL_LOOPING, AL_TRUE);
	}

	// GeneralsX @bugfix Mr. Meeseeks 19/06/2026 - Initialize stream volume
	Real desiredVolume = event->getVolume() * event->getVolumeShift();
	if (event->getAudioEventInfo() && event->getAudioEventInfo()->m_soundType == AT_Music) {
		alSourcef(stream->getSource(), AL_GAIN, m_musicVolume * desiredVolume);
	}
	else {
		alSourcef(stream->getSource(), AL_GAIN, m_speechVolume * desiredVolume);
	}

	stream->play();
	if (event->getAudioEventInfo()->m_soundType == AT_Music) {
		// Need to stop/fade out the old music here.
	}
}

//-------------------------------------------------------------------------------------------------
ALuint OpenALAudioManager::playSample(AudioEventRTS* event, PlayingAudio* audio)
{
	// Load the file in
	ALuint bufferHandle = loadBufferForRead(event);
	if (bufferHandle) {
		alSourcei(audio->m_source, AL_SOURCE_RELATIVE, AL_TRUE);
		alSourcei(audio->m_source, AL_BUFFER, (ALuint)(uintptr_t)bufferHandle);
		// GeneralsX @bugfix Mr. Meeseeks 19/06/2026 - Initialize sample volume
		adjustPlayingVolume(audio);
		alSourcePlay(audio->m_source);
	}

	return bufferHandle;
}

//-------------------------------------------------------------------------------------------------
ALuint OpenALAudioManager::playSample3D(AudioEventRTS* event, PlayingAudio* sample3D)
{
	const Coord3D* pos = getCurrentPositionFromEvent(event);
	if (pos) {
		ALuint handle = loadBufferForRead(event);
		const AudioSettings* audioSettings = getAudioSettings();

		if (handle) {
			auto source = sample3D->m_source;
			Real x = pos->x;
			Real y = pos->y;
			Real z = pos->z;
			Real pitch = event->getPitchShift() != 0.0f ? event->getPitchShift() : 1.0f;
			ALint channels = 0;
			alGetBufferi(handle, AL_CHANNELS, &channels);
			alSourcef(source, AL_PITCH, pitch);

			if (channels > 1) {
				// GeneralsX @bugfix Bender 09/05/2026 Fallback multichannel positional voice assets to direct playback instead of dropping them.
				alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
				alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
				alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
				alSourcef(source, AL_ROLLOFF_FACTOR, 0.0f);
			#ifdef AL_DIRECT_CHANNELS_SOFT
				alSourcei(source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
			#endif
			#ifdef AL_SOURCE_SPATIALIZE_SOFT
				alSourcei(source, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);
			#endif
				DEBUG_LOG(("OpenAL positional fallback active for '%s' (%d channels)\n",
					event->getEventName().str(), channels));
			}
			else {
				alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
				// Set the position values of the sample here
				if (event->getAudioEventInfo()->m_type & ST_GLOBAL) {
					alSourcef(source, AL_REFERENCE_DISTANCE, audioSettings->m_globalMinRange);
					alSourcef(source, AL_MAX_DISTANCE, audioSettings->m_globalMaxRange);
				}
				else {
					alSourcef(source, AL_REFERENCE_DISTANCE, event->getAudioEventInfo()->m_minDistance);
					alSourcef(source, AL_MAX_DISTANCE, event->getAudioEventInfo()->m_maxDistance);
				}

				alSourcef(source, AL_ROLLOFF_FACTOR, 0.5f);
				alSource3f(source, AL_POSITION, x, y, z);
			}
			alSourcei(source, AL_BUFFER, handle);
			DEBUG_LOG(("Playing 3D sample '%s' at %f, %f, %f\n", event->getEventName().str(), x, y, z));

			// GeneralsX @bugfix Mr. Meeseeks 19/06/2026 - Initialize 3D sample volume
			adjustPlayingVolume(sample3D);

			// Start playback
			alSourcePlay(source);
		}
		return handle;
	}

	return 0;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::enumerateDevices(void)
{
	const ALCchar* devices = NULL;
	if (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT") == AL_TRUE) {
		devices = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
		if ((devices == nullptr || *devices == '\0') && alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT") == AL_TRUE) {
			devices = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
		}
	}

	if (devices == nullptr) {
		DEBUG_LOG(("Enumerating OpenAL devices is not supported"));
		return;
	}

	const ALCchar* device = devices;
	const ALCchar* next = devices + 1;
	size_t len = 0;
	size_t idx = 0;
	while (device && *device != '\0' && next && *next != '\0' && idx < AL_MAX_PLAYBACK_DEVICES) {
		m_alDevicesList[idx++] = device;
		len = strlen(device);
		device += (len + 1);
		next += (len + 2);
	}

	m_alMaxDevicesIndex = idx;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::createListener(void)
{
	// OpenAL only has one listener
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::initDelayFilter(void)
{
}

//-------------------------------------------------------------------------------------------------
Bool OpenALAudioManager::isValidProvider(void)
{
	return (m_selectedProvider < m_providerCount);
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::initSamplePools(void)
{
	if (!(isOn(AudioAffect_Sound3D) && isValidProvider()))
	{
		return;
	}

	m_num2DSamples = getAudioSettings()->m_sampleCount2D;
	m_num3DSamples = getAudioSettings()->m_sampleCount3D;

	// Streams are basically free, so we can just allocate the appropriate number
	m_numStreams = getAudioSettings()->m_streamCount;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::processRequest(AudioRequest* req)
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
void* OpenALAudioManager::getHandleForBink(void)
{
	if (!m_binkAudio) {
		DEBUG_LOG(("Creating Bink audio stream\n"));
		m_binkAudio = NEW OpenALAudioStream;
	}
	return m_binkAudio;
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::releaseHandleForBink(void)
{
	if (m_binkAudio) {
		DEBUG_LOG(("Releasing Bink audio stream\n"));
		delete m_binkAudio;
		m_binkAudio = NULL;
	}
}

//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::friend_forcePlayAudioEventRTS(const AudioEventRTS* eventToPlay)
{
	if (!eventToPlay->getAudioEventInfo()) {
		getInfoForAudioEvent(eventToPlay);
		if (!eventToPlay->getAudioEventInfo()) {
			DEBUG_CRASH(("No info for forced audio event '%s'\n", eventToPlay->getEventName().str()));
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

	// GeneralsX @bugfix BenderAI 11/03/2026 - heap-allocate so releasePlayingAudio can safely
	// delete it via m_cleanupAudioEventRTS. Stack allocation caused SIGSEGV in delete.
	AudioEventRTS* event = NEW AudioEventRTS(*eventToPlay);

	event->generateFilename();
	event->generatePlayInfo();

	std::list<std::pair<AsciiString, Real> >::iterator it;
	for (it = m_adjustedVolumes.begin(); it != m_adjustedVolumes.end(); ++it) {
		if (it->first == event->getEventName()) {
			event->setVolume(it->second);
			break;
		}
	}

	playAudioEvent(event);
}

#if defined(_DEBUG) || defined(_INTERNAL)
//-------------------------------------------------------------------------------------------------
void OpenALAudioManager::dumpAllAssetsUsed()
{
	if (!TheGlobalData->m_preloadReport) {
		return;
	}

	// Dump all the audio assets we've used.
	FILE* logfile = fopen("PreloadedAssets.txt", "a+");	//append to log
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
			AudioEventInfo* aei = findAudioEventInfo(astr);
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
	logfile = NULL;
}
#endif

//-------------------------------------------------------------------------------------------------
UnsignedInt OpenALAudioManager::getNumAvailable2DSamples() const
{
	// GeneralsX @bugfix Mr. Meeseeks 27/06/2026 Fix available samples calculation to prevent voicelines culling
	UnsignedInt playing = (UnsignedInt)m_playingSounds.size();
	return (m_num2DSamples > playing) ? (m_num2DSamples - playing) : 0;
}

//-------------------------------------------------------------------------------------------------
UnsignedInt OpenALAudioManager::getNumAvailable3DSamples() const
{
	// GeneralsX @bugfix Mr. Meeseeks 27/06/2026 Fix available samples calculation to prevent voicelines culling
	UnsignedInt playing = (UnsignedInt)m_playing3DSounds.size();
	return (m_num3DSamples > playing) ? (m_num3DSamples - playing) : 0;
}
