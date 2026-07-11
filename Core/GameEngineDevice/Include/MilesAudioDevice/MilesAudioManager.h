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

// FILE: MilesAudioManager.h //////////////////////////////////////////////////////////////////////////
// MilesAudioManager implementation
// Author: John K. McDonald, July 2002

#include "Common/AsciiString.h"
#include "Common/GameAudio.h"
#include "mss/mss.h"
#include "mutex.h"

class AudioEventRTS;

enum { MAXPROVIDERS = 64 };

enum PlayingAudioType CPP_11(: Int)
{
	PAT_Sample,
	PAT_3DSample,
	PAT_Stream,
	PAT_INVALID
};

enum PlayingStatus CPP_11(: Int)
{
	PS_Playing,
	PS_Stopping, ///< Is about to be stopped
	PS_Stopped, ///< Is about to be released
};

enum PlayingWhich CPP_11(: Int)
{
	PW_Attack,
	PW_Sound,
	PW_Decay,
	PW_INVALID
};

struct PlayingAudio
{
	union
	{
		HSAMPLE m_sample;
		H3DSAMPLE m_3DSample;
		HSTREAM m_stream;
	};

	AudioEventRTS *m_audioEventRTS;
	void *m_file; // The file that was opened to play this
	PlayingAudioType m_type;
	volatile PlayingStatus m_status; // This member is adjusted by another running thread.
	Short m_framesFaded;
	Bool m_fade;
	Bool m_cleanupAudioEventRTS;

	PlayingAudio()
		: m_sample(nullptr)
		, m_audioEventRTS(nullptr)
		, m_file(nullptr)
		, m_type(PAT_INVALID)
		, m_status(PS_Playing)
		, m_framesFaded(0)
		, m_fade(false)
		, m_cleanupAudioEventRTS(true)
	{}

	static_assert(sizeof(m_status) == sizeof(long), "Must be size of long, because it is used with Interlocked functions");
};

struct ProviderInfo
{
  AsciiString name;
  HPROVIDER id;
	Bool m_isValid;
};

struct OpenAudioFile
{
	AILSOUNDINFO m_soundInfo;
	void *m_file;
	UnsignedInt m_openCount;
	UnsignedInt m_fileSize;

	Bool m_compressed;	// if the file was compressed, then we need to free it with a miles function.

	// Note: OpenAudioFile does not own this m_eventInfo, and should not delete it.
	const AudioEventInfo *m_eventInfo;	// Not mutable, unlike the one on AudioEventRTS.
};

typedef std::hash_map< AsciiString, OpenAudioFile, rts::hash<AsciiString>, rts::equal_to<AsciiString>/**/> OpenFilesHash;
typedef OpenFilesHash::iterator OpenFilesHashIt;

class AudioFileCache
{
	public:
		AudioFileCache();

		// Protected by mutex
		virtual ~AudioFileCache();
		void *openFile( AudioEventRTS *eventToOpenFrom );
		void closeFile( void *fileToClose );
		void setMaxSize( UnsignedInt size );
		// End Protected by mutex

		// Note: These functions should be used for informational purposes only. For speed reasons,
		// they are not protected by the mutex, so they are not guaranteed to be valid if called from
		// outside the audio cache. They should be used as a rough estimate only.
		UnsignedInt getCurrentlyUsedSize() const { return m_currentlyUsedSize; }
		UnsignedInt getMaxSize() const { return m_maxSize; }

	protected:
		void releaseOpenAudioFile( OpenAudioFile *fileToRelease );

		// This function will return TRUE if it was able to free enough space, and FALSE otherwise.
		Bool freeEnoughSpaceForSample(const OpenAudioFile& sampleThatNeedsSpace);

		OpenFilesHash m_openFiles;
		UnsignedInt m_currentlyUsedSize;
		UnsignedInt m_maxSize;
		HANDLE m_mutex;
		const char *m_mutexName;
};

class MilesAudioManager : public AudioManager
{

	public:
#if defined(RTS_DEBUG)
		virtual void audioDebugDisplay(DebugDisplayInterface *dd, void *, FILE *fp = nullptr ) override;
		virtual AudioHandle addAudioEvent( const AudioEventRTS *eventToAdd ) override; ///< Add an audio event (event must be declared in an INI file)
#endif

		// from AudioDevice
		virtual void init() override;
		virtual void postProcessLoad() override;
		virtual void reset() override;
		virtual void update() override;

		MilesAudioManager();
		virtual ~MilesAudioManager() override;

		virtual AsciiString nextMusicTrack() override;
		virtual AsciiString prevMusicTrack() override;
		virtual Bool isMusicPlaying() const override;
		virtual Bool hasMusicTrackCompleted( const AsciiString& trackName, Int numberOfTimes ) const override;

		virtual void openDevice() override;
		virtual void closeDevice() override;
		virtual void *getDevice() override { return m_digitalHandle; }

		virtual void stopAudio( AudioAffect which ) override;
		virtual void pauseAudio( AudioAffect which ) override;
		virtual void resumeAudio( AudioAffect which ) override;
		virtual void pauseAmbient( Bool shouldPause ) override;

		virtual void killAudioEventImmediately( AudioHandle audioEvent ) override;

		///< Return whether the current audio is playing or not.
		///< NOTE NOTE NOTE !!DO NOT USE THIS IN FOR GAMELOGIC PURPOSES!! NOTE NOTE NOTE
		virtual Bool isCurrentlyPlaying( AudioHandle handle ) override;

		virtual void notifyOfAudioCompletion( UnsignedInt handle, UnsignedInt flags ) override;
		virtual PlayingAudio *findPlayingAudioFrom( UnsignedInt handle, UnsignedInt flags );

		virtual UnsignedInt getProviderCount() const override;
		virtual AsciiString getProviderName( UnsignedInt providerNum ) const override;
		virtual UnsignedInt getProviderIndex( AsciiString providerName ) const override;
		virtual void selectProvider( UnsignedInt providerNdx ) override;
		virtual void unselectProvider() override;
		virtual UnsignedInt getSelectedProvider() const override;
		virtual void setSpeakerType( UnsignedInt speakerType ) override;
		virtual UnsignedInt getSpeakerType() override;

 		virtual void *getHandleForBink() override;
 		virtual void releaseHandleForBink() override;

		virtual void friend_forcePlayAudioEventRTS(const AudioEventRTS* eventToPlay) override;

		virtual UnsignedInt getNum2DSamples() const override;
		virtual UnsignedInt getNum3DSamples() const override;
		virtual UnsignedInt getNumStreams() const override;
		virtual UnsignedInt getNumAvailable2DSamples() const override;
		virtual UnsignedInt getNumAvailable3DSamples() const override;

		virtual Bool doesViolateLimit( AudioEventRTS *event ) const override;
		virtual Bool isPlayingLowerPriority( AudioEventRTS *event ) const override;
		virtual Bool isPlayingAlready( AudioEventRTS *event ) const override;
		virtual Bool isObjectPlayingVoice( UnsignedInt objID ) const override;
		Bool killLowestPrioritySoundImmediately( AudioEventRTS *event );
		AudioEventRTS* findLowestPrioritySound( AudioEventRTS *event );

		virtual void adjustVolumeOfPlayingAudio(AsciiString eventName, Real newVolume) override;

		virtual void removePlayingAudio( AsciiString eventName ) override;
		virtual void removeAllDisabledAudio() override;

		virtual void processRequestList() override;
		virtual void processPlayingList();
		virtual void processFadingList();

		Bool shouldProcessRequestThisFrame( AudioRequest *req ) const;
		void adjustRequest( AudioRequest *req );
		Bool checkForSample( AudioRequest *req );

		virtual void setHardwareAccelerated(Bool accel) override;
		virtual void setSpeakerSurround(Bool surround) override;

		virtual void setPreferredProvider(AsciiString provider) override { m_pref3DProvider = provider; }
		virtual void setPreferredSpeaker(AsciiString speakerType) override { m_prefSpeaker = speakerType; }

		virtual Real getFileLengthMS( AsciiString strToLoad ) const override;

		virtual void closeAnySamplesUsingFile( const void *fileToClose ) override;


    virtual Bool has3DSensitiveStreamsPlaying() const override;


	protected:
		// 3-D functions
		virtual void setDeviceListenerPosition() override;
		const Coord3D *getCurrentPositionFromEvent( AudioEventRTS *event );
		Bool isOnScreen( const Coord3D *pos ) const;
		Real getEffectiveVolume(AudioEventRTS *event) const;

		// Looping functions
		Bool startNextLoop( PlayingAudio *looping );

		void playStream( AudioEventRTS *event, HSTREAM stream );
		// Returns the file handle for attachment to the PlayingAudio structure
		void *playSample( AudioEventRTS *event, HSAMPLE sample );
		void *playSample3D( AudioEventRTS *event, H3DSAMPLE sample3D );

		void buildProviderList();
		void createListener();
		void initDelayFilter();
		Bool isValidProvider();
		void initSamplePools();
		void processRequest( AudioRequest *req );

		void playAudioEvent( AudioEventRTS *event );
		void stopAudioEvent( AudioHandle handle );
		void pauseAudioEvent( AudioHandle handle );

		void *loadFileForRead( AudioEventRTS *eventToLoadFrom );
		void closeFile( void *fileRead );

		PlayingAudio *allocatePlayingAudio();
		void releaseMilesHandles( PlayingAudio *release );
		void releasePlayingAudio( PlayingAudio *release );
		void stopPlayingAudio( PlayingAudio *release );
		void fadePlayingAudio( PlayingAudio *playing );

		PlayingAudio *findActiveMusic( const AsciiString *trackName = nullptr );
		const PlayingAudio *findActiveMusic( const AsciiString* trackName = nullptr ) const;

		void releasePlayingAudioInListIfStopped(std::list<PlayingAudio *> &list, CriticalSectionClass &cs);
		void stopAllAudioImmediately();
		void freeAllMilesHandles();

		HSAMPLE getAvailable2DSample( AudioEventRTS *event );
		H3DSAMPLE getAvailable3DSample( AudioEventRTS *event );

		void adjustPlayingVolume( PlayingAudio *audio );

		void stopAllSpeech();

		void initFilters( HSAMPLE sample, AudioEventRTS *eventInfo );
		void initFilters3D( H3DSAMPLE sample, AudioEventRTS *eventInfo, const Coord3D *pos );

	protected:
		ProviderInfo m_provider3D[MAXPROVIDERS];
		UnsignedInt m_providerCount;
		UnsignedInt m_selectedProvider;
		UnsignedInt m_lastProvider;
		UnsignedInt m_selectedSpeakerType;

		AsciiString m_pref3DProvider;
		AsciiString m_prefSpeaker;

		HDIGDRIVER m_digitalHandle;
		H3DPOBJECT m_listener;
		HPROVIDER m_delayFilter;

		// This is a list of all handles that are forcibly played. They always play as UI sounds.
		std::list<HAUDIO> m_audioForcePlayed;

		// Available handles for play. Note that there aren't handles open in advance for
		// streaming things, only 2-D and 3-D sounds.
		std::vector<HSAMPLE> m_availableSamples;
		std::vector<H3DSAMPLE> m_available3DSamples;

		// Currently Playing audio. Useful if we have to preempt it.
		// This should rarely if ever happen, as we mirror this in Sounds, and attempt to
		// keep preemption from taking place here.
		std::list<PlayingAudio *> m_playingSounds;
		std::list<PlayingAudio *> m_playing3DSounds;
		std::list<PlayingAudio *> m_playingStreams;

		// Currently fading music. We just let it finish fading, then release it.
		std::list<PlayingAudio *> m_fadingAudio;

		// TheSuperHackers @fix Erasing from PlayingAudio lists on main thread is not safe when the MSS Timer thread
		// needs to iterate the same lists. The critical sections are used to guard writes that will invalidate iterators.
		CriticalSectionClass m_playingSoundsCS;
		CriticalSectionClass m_playing3DSoundsCS;
		CriticalSectionClass m_playingStreamsCS;
		CriticalSectionClass m_fadingAudioCS;

		AudioFileCache *m_audioCache;
		PlayingAudio *m_binkHandle;
		UnsignedInt m_num2DSamples;
		UnsignedInt m_num3DSamples;
		UnsignedInt m_numStreams;

#if defined(RTS_DEBUG)
		typedef std::set<AsciiString> SetAsciiString;
		typedef SetAsciiString::iterator SetAsciiStringIt;
		SetAsciiString m_allEventsLoaded;
		void dumpAllAssetsUsed();
#endif

};

// TheSuperHackers @feature helmutbuhler 17/05/2025 AudioManager that does almost nothing. Useful for headless mode.
// @bugfix Caball009 26/03/2026 Scripts may require the actual audio file length to function properly, which is important for the CRC computation.
// The Miles AudioManager handles the device opening / closure, so that getFileLengthMS can function as intended.
class MilesAudioManagerDummy : public MilesAudioManager
{
#if defined(RTS_DEBUG)
	virtual void audioDebugDisplay(DebugDisplayInterface* dd, void* userData, FILE* fp) override {}
#endif
	virtual void stopAudio(AudioAffect which) override {}
	virtual void pauseAudio(AudioAffect which) override {}
	virtual void resumeAudio(AudioAffect which) override {}
	virtual void pauseAmbient(Bool shouldPause) override {}
	virtual void killAudioEventImmediately(AudioHandle audioEvent) override {}
	virtual AsciiString nextMusicTrack() override { return AsciiString::TheEmptyString; }
	virtual AsciiString prevMusicTrack() override { return AsciiString::TheEmptyString; }
	virtual Bool isMusicPlaying() const override { return false; }
	virtual Bool hasMusicTrackCompleted(const AsciiString& trackName, Int numberOfTimes) const override { return false; }
	//virtual void openDevice() override {}
	//virtual void closeDevice() override {}
	//virtual void* getDevice() override { return nullptr; }
	virtual void notifyOfAudioCompletion(UnsignedInt audioCompleted, UnsignedInt flags) override {}
	virtual UnsignedInt getProviderCount() const override { return 0; };
	virtual AsciiString getProviderName(UnsignedInt providerNum) const override { return AsciiString::TheEmptyString; }
	virtual UnsignedInt getProviderIndex(AsciiString providerName) const override { return 0; }
	virtual void selectProvider(UnsignedInt providerNdx) override {}
	virtual void unselectProvider() override {}
	virtual UnsignedInt getSelectedProvider() const override { return 0; }
	virtual void setSpeakerType(UnsignedInt speakerType) override {}
	virtual UnsignedInt getSpeakerType() override { return 0; }
	virtual UnsignedInt getNum2DSamples() const override { return 0; }
	virtual UnsignedInt getNum3DSamples() const override { return 0; }
	virtual UnsignedInt getNumStreams() const override { return 0; }
	virtual UnsignedInt getNumAvailable2DSamples() const override { return 0; }
	virtual UnsignedInt getNumAvailable3DSamples() const override { return 0; }
	virtual Bool doesViolateLimit(AudioEventRTS* event) const override { return false; }
	virtual Bool isPlayingLowerPriority(AudioEventRTS* event) const override { return false; }
	virtual Bool isPlayingAlready(AudioEventRTS* event) const override { return false; }
	virtual Bool isObjectPlayingVoice(UnsignedInt objID) const override { return false; }
	virtual void adjustVolumeOfPlayingAudio(AsciiString eventName, Real newVolume) override {}
	virtual void removePlayingAudio(AsciiString eventName) override {}
	virtual void removeAllDisabledAudio() override {}
	virtual Bool has3DSensitiveStreamsPlaying() const override { return false; }
	virtual void* getHandleForBink() override { return nullptr; }
	virtual void releaseHandleForBink() override {}
	virtual void friend_forcePlayAudioEventRTS(const AudioEventRTS* eventToPlay) override {}
	virtual void setPreferredProvider(AsciiString providerNdx) override {}
	virtual void setPreferredSpeaker(AsciiString speakerType) override {}
	//virtual Real getFileLengthMS(AsciiString strToLoad) const override { return 0.0f; }
	virtual void closeAnySamplesUsingFile(const void* fileToClose) override {}
	virtual void setDeviceListenerPosition() override {}
};
