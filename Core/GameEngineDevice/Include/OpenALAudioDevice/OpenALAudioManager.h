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

// FILE: OpenALAudioManager.h //////////////////////////////////////////////////////////////////////////
// OpenALAudioManager implementation
// Author: Stephan Vedder, March 2025
#pragma once
#include "Common/AsciiString.h"
#include "Common/GameAudio.h"
#include <AL/al.h>
#include <AL/alc.h>

class AudioEventRTS;

enum
{
	MAXPROVIDERS = 64
};

#define AL_MAX_PLAYBACK_DEVICES 64

enum PlayingAudioType
{
	PAT_Sample,
	PAT_3DSample,
	PAT_Stream,
	PAT_INVALID
};

enum PlayingWhich
{
	PW_Attack,
	PW_Sound,
	PW_Decay,
	PW_INVALID
};

struct ProviderInfo
{
	AsciiString name;
	Bool m_isValid;
};

struct PlayingAudio;
struct OpenALAudioFileCache;
class OpenALAudioStream;

class OpenALAudioManager : public AudioManager
{
friend class OpenALAudioStream;
friend class FFmpegVideoStream;
friend class OpenALAudioFileCache;

public:
#if defined(_DEBUG) || defined(_INTERNAL)
	virtual void audioDebugDisplay(DebugDisplayInterface *dd, void *, FILE *fp = NULL);
	virtual AudioHandle addAudioEvent(const AudioEventRTS *eventToAdd); ///< Add an audio event (event must be declared in an INI file)
#endif

	// from AudioDevice
	virtual void init();
	virtual void postProcessLoad();
	virtual void reset();
	virtual void update();

	OpenALAudioManager();
	virtual ~OpenALAudioManager();

	virtual AsciiString nextMusicTrack(void);
	virtual AsciiString prevMusicTrack(void);
	virtual Bool isMusicPlaying(void) const;
	virtual Bool hasMusicTrackCompleted(const AsciiString &trackName, Int numberOfTimes) const;

	virtual void openDevice(void);
	virtual void closeDevice(void);
	virtual void *getDevice(void) { return m_alcDevice; }

	virtual void stopAudio(AudioAffect which);
	virtual void pauseAudio(AudioAffect which);
	virtual void resumeAudio(AudioAffect which);
	virtual void pauseAmbient(Bool shouldPause);

	virtual void killAudioEventImmediately(AudioHandle audioEvent);

	///< Return whether the current audio is playing or not.
	///< NOTE NOTE NOTE !!DO NOT USE THIS IN FOR GAMELOGIC PURPOSES!! NOTE NOTE NOTE
	virtual Bool isCurrentlyPlaying(AudioHandle handle);

	virtual void notifyOfAudioCompletion(ALuint source, UnsignedInt flags);
	virtual PlayingAudio *findPlayingAudioFrom(ALuint source, UnsignedInt flags);

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
	virtual void removeAllDisabledAudio();

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

	virtual void closeAnySamplesUsingFile(const void *fileToClose) override;

	virtual Bool has3DSensitiveStreamsPlaying(void) const;

protected:
	// 3-D functions
	virtual void setDeviceListenerPosition(void);
	const Coord3D *getCurrentPositionFromEvent(AudioEventRTS *event);
	Bool isOnScreen(const Coord3D *pos) const;
	Real getEffectiveVolume(AudioEventRTS *event) const;

	// Looping functions
	Bool startNextLoop(PlayingAudio *looping);

	void playStream(AudioEventRTS *event, OpenALAudioStream* stream);
	// Returns the buffer handle representing audio data for attachment to the PlayingAudio structure
	ALuint playSample(AudioEventRTS *event, PlayingAudio *audio);
	ALuint playSample3D(AudioEventRTS *event, PlayingAudio * audio);

protected:
	void enumerateDevices(void);
	void createListener(void);
	void initDelayFilter(void);
	Bool isValidProvider(void);
	void initSamplePools(void);
	void processRequest(AudioRequest *req);

	void playAudioEvent(AudioEventRTS *event);
	void stopAudioEvent(AudioHandle handle);
	void pauseAudioEvent(AudioHandle handle);

	ALuint loadBufferForRead(AudioEventRTS *eventToLoadFrom);
	void closeBuffer(ALuint bufferToClose);

	PlayingAudio *allocatePlayingAudio(void);
	void releaseOpenALHandles(PlayingAudio *release);
	void releasePlayingAudio(PlayingAudio *release);

	void stopAllAudioImmediately(void);
	void freeAllOpenALHandles(void);

	PlayingAudio *getFirst2DSample(AudioEventRTS *event);
	PlayingAudio *getFirst3DSample(AudioEventRTS *event);

	void adjustPlayingVolume(PlayingAudio *audio);

	void stopAllSpeech(void);
    static ALenum getALFormat(uint8_t channels, uint8_t bitsPerSample);
protected:
	AsciiString m_alDevicesList[AL_MAX_PLAYBACK_DEVICES];
	int m_alMaxDevicesIndex;

	ProviderInfo m_provider3D[MAXPROVIDERS];
	UnsignedInt m_providerCount;
	UnsignedInt m_selectedProvider;
	UnsignedInt m_lastProvider;
	UnsignedInt m_selectedSpeakerType;

	AsciiString m_pref3DProvider;
	AsciiString m_prefSpeaker;

	// Currently Playing stuff. Useful if we have to preempt it.
	// This should rarely if ever happen, as we mirror this in Sounds, and attempt to
	// keep preemption from taking place here.
	std::list<PlayingAudio *> m_playingSounds;
	std::list<PlayingAudio *> m_playing3DSounds;
	std::list<PlayingAudio *> m_playingStreams;

	// Currently fading stuff. At this point, we just want to let it finish fading, when it is
	// done it should be added to the completed list, then "freed" and the counts should be updated
	// on the next update
	std::list<PlayingAudio *> m_fadingAudio;

	// Stuff that is done playing (either because it has finished or because it was killed)
	// This stuff should be cleaned up during the next update cycle. This includes updating counts
	// in the sound engine
	std::list<PlayingAudio *> m_stoppedAudio;

	OpenALAudioFileCache *m_audioCache;
	UnsignedInt m_num2DSamples;
	UnsignedInt m_num3DSamples;
	UnsignedInt m_numStreams;

#if defined(_DEBUG) || defined(_INTERNAL)
	typedef std::set<AsciiString> SetAsciiString;
	typedef SetAsciiString::iterator SetAsciiStringIt;
	SetAsciiString m_allEventsLoaded;
	void dumpAllAssetsUsed();
#endif

	ALCdevice *m_alcDevice = nullptr;
	ALCcontext *m_alcContext = nullptr;
	OpenALAudioStream* m_binkAudio = nullptr;
};
