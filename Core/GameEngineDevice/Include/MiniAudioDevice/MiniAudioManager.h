/*
**	Command & Conquer Generals(tm)
**	Copyright 2026 Stephan Vedder
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

// FILE: MiniAudioManager.h //////////////////////////////////////////////////////////////////////////
// MiniAudioManager implementation
// Original Author: Stephan Vedder (feliwir), June 2026
// https://github.com/feliwir/CnC_Generals_Zero_Hour
// Adapted for GeneralsX Core architecture
#pragma once

#include "Common/AsciiString.h"
#include "Common/GameAudio.h"
#include "MiniAudioDevice/MiniAudioCompat.h"

class AudioEventRTS;

enum { MAXPROVIDERS = 64 };

enum PlayingAudioType
{
	PAT_Sample,
	PAT_3DSample,
	PAT_Stream,
	PAT_INVALID
};

struct PlayingAudio
{
	ma_sound* m_sound;
	ma_audio_buffer* m_audioBuffer;
	PlayingAudioType m_type;
	AudioEventRTS* m_audioEventRTS;
	Bool m_requestStop;
	Bool m_cleanupAudioEventRTS;
	Int m_framesFaded;

	PlayingAudio() :
		m_sound(NULL),
		m_audioBuffer(NULL),
		m_type(PAT_INVALID),
		m_audioEventRTS(NULL),
		m_requestStop(false),
		m_cleanupAudioEventRTS(true),
		m_framesFaded(0)
	{ }
};

class MiniAudioManager : public AudioManager
{
public:
#if defined(_DEBUG) || defined(_INTERNAL)
	virtual void audioDebugDisplay(DebugDisplayInterface *dd, void *, FILE *fp = NULL);
	virtual AudioHandle addAudioEvent(const AudioEventRTS *eventToAdd);
#endif

	// from AudioDevice
	virtual void init();
	virtual void postProcessLoad();
	virtual void reset();
	virtual void update();

	MiniAudioManager();
	virtual ~MiniAudioManager();

	virtual AsciiString nextMusicTrack(void);
	virtual AsciiString prevMusicTrack(void);
	virtual Bool isMusicPlaying(void) const;
	virtual Bool hasMusicTrackCompleted(const AsciiString& trackName, Int numberOfTimes) const;
	virtual AsciiString getMusicTrackName(void) const;

	virtual void openDevice(void);
	virtual void closeDevice(void);
	virtual void *getDevice(void) { return &m_engine; }

	virtual void stopAudio(AudioAffect which);
	virtual void pauseAudio(AudioAffect which);
	virtual void resumeAudio(AudioAffect which);
	virtual void pauseAmbient(Bool shouldPause);

	virtual void killAudioEventImmediately(AudioHandle audioEvent);

	virtual void stopAllAmbientsBy(Object *objID);
	virtual void stopAllAmbientsBy(Drawable *drawID);

	///< Return whether the current audio is playing or not.
	///< NOTE NOTE NOTE !!DO NOT USE THIS IN FOR GAMELOGIC PURPOSES!! NOTE NOTE NOTE
	virtual Bool isCurrentlyPlaying(AudioHandle handle);

	virtual void notifyOfAudioCompletion(UnsignedInt audioCompleted, UnsignedInt flags);
	virtual PlayingAudio *findPlayingAudioFrom(UnsignedInt audioCompleted, UnsignedInt flags);

	virtual UnsignedInt getProviderCount(void) const;
	virtual AsciiString getProviderName(UnsignedInt providerNum) const;
	virtual UnsignedInt getProviderIndex(AsciiString providerName) const;
	virtual void selectProvider(UnsignedInt providerNdx);
	virtual void unselectProvider(void);
	virtual UnsignedInt getSelectedProvider(void) const;
	virtual void setSpeakerType(UnsignedInt speakerType);
	virtual UnsignedInt getSpeakerType(void);

	virtual void *getHandleForBink(void);
	virtual void releaseHandleForBink(void);

	virtual void friend_forcePlayAudioEventRTS(const AudioEventRTS *eventToPlay);

	virtual UnsignedInt getNum2DSamples(void) const;
	virtual UnsignedInt getNum3DSamples(void) const;
	virtual UnsignedInt getNumStreams(void) const;
	virtual UnsignedInt getNumAvailable2DSamples() const;
	virtual UnsignedInt getNumAvailable3DSamples() const;

	virtual Bool doesViolateLimit(AudioEventRTS *event) const;
	virtual Bool isPlayingLowerPriority(AudioEventRTS *event) const;
	virtual Bool isPlayingAlready(AudioEventRTS *event) const;
	virtual Bool isObjectPlayingVoice(UnsignedInt objID) const;
	Bool killLowestPrioritySoundImmediately(AudioEventRTS *event);
	AudioEventRTS *findLowestPrioritySound(AudioEventRTS *event);

	virtual void adjustVolumeOfPlayingAudio(AsciiString eventName, Real newVolume);

	virtual void removePlayingAudio(AsciiString eventName);
	virtual void removeAllDisabledAudio(void);

	virtual void processRequestList(void);
	virtual void processPlayingList(void);
	virtual void processFadingList(void);
	virtual void processStoppedList(void);

	Bool shouldProcessRequestThisFrame(AudioRequest *req) const;
	void adjustRequest(AudioRequest *req);
	Bool checkForSample(AudioRequest *req);

	virtual void setHardwareAccelerated(Bool accel);
	virtual void setSpeakerSurround(Bool surround);

	virtual void setPreferredProvider(AsciiString provider) { m_pref3DProvider = provider; }
	virtual void setPreferredSpeaker(AsciiString speakerType) { m_prefSpeaker = speakerType; }

	virtual Real getFileLengthMS(AsciiString strToLoad) const;

	virtual void closeAnySamplesUsingFile(const void *fileToClose);

	virtual Bool has3DSensitiveStreamsPlaying(void) const;

protected:
	// 3-D functions
	virtual void setDeviceListenerPosition(void);
	const Coord3D *getCurrentPositionFromEvent(AudioEventRTS *event);
	Bool isOnScreen(const Coord3D *pos) const;
	Real getEffectiveVolume(AudioEventRTS *event) const;

	// Looping functions
	Bool startNextLoop(PlayingAudio *looping);

	void playStream(AudioEventRTS *event, ma_sound *sound);

protected:
	void buildProviderList(void);
	void createListener(void);
	void initDelayFilter(void);
	Bool isValidProvider(void);
	void initSamplePools(void);
	void processRequest(AudioRequest *req);

	void playAudioEvent(AudioEventRTS *event);
	void stopAudioEvent(AudioHandle handle);
	void pauseAudioEvent(AudioHandle handle);

	PlayingAudio *allocatePlayingAudio(void);
	void releaseMiniAudioHandles(PlayingAudio *release);
	void releasePlayingAudio(PlayingAudio *release);

	void stopAllAudioImmediately(void);
	void freeAllMiniAudioHandles(void);

	void adjustPlayingVolume(PlayingAudio *audio);

	void stopAllSpeech(void);

protected:
	ma_device_info* m_playbackDevices;
	ma_uint32 m_playbackDeviceCount;
	UnsignedInt m_selectedPlaybackDevice;
	UnsignedInt m_lastSelectedPlaybackDevice;
	UnsignedInt m_selectedSpeakerType;

	AsciiString m_pref3DProvider;
	AsciiString m_prefSpeaker;

	ma_engine m_engine;
	ma_context m_context;
	ma_resource_manager m_resourceManager;
	ma_log m_log;
	ma_sound_group m_musicGroup;
	ma_sound_group m_soundGroup;
	ma_sound_group m_sound3DGroup;
	ma_sound_group m_speechGroup;

	// Currently Playing stuff. Useful if we have to preempt it.
	// This should rarely if ever happen, as we mirror this in Sounds, and attempt to
	// keep preemption from taking place here.
	std::list<PlayingAudio *> m_playingSounds;

	// Currently fading stuff. At this point, we just want to let it finish fading, when it is
	// done it should be added to the completed list, then "freed" and the counts should be updated
	// on the next update
	std::list<PlayingAudio *> m_fadingAudio;

	// Stuff that is done playing (either because it has finished or because it was killed)
	// This stuff should be cleaned up during the next update cycle. This includes updating counts
	// in the sound engine
	std::list<PlayingAudio *> m_stoppedAudio;

	void *m_binkHandle;

#if defined(_DEBUG) || defined(_INTERNAL)
	typedef std::set<AsciiString> SetAsciiString;
	typedef SetAsciiString::iterator SetAsciiStringIt;
	SetAsciiString m_allEventsLoaded;
	void dumpAllAssetsUsed();
#endif
};
