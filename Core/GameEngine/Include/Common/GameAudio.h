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

//----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//----------------------------------------------------------------------------
//
// Project:    RTS 3
//
// File name:  Common/GameAudio.h
//
// Created:    5/01/01
//
//----------------------------------------------------------------------------

#pragma once

// Includes
#include "Lib/BaseType.h"
#include "Common/STLTypedefs.h"
#include "Common/SubsystemInterface.h"


// Forward Declarations

class AsciiString;
class AudioEventRTS;
class DebugDisplayInterface;
class Drawable;
class MusicManager;
class Object;
class SoundManager;


enum AudioAffect CPP_11(: Int);
enum AudioType CPP_11(: Int);

struct AudioEventInfo;
struct AudioRequest;
struct AudioSettings;
struct MiscAudio;

typedef std::hash_map<AsciiString, AudioEventInfo*, rts::hash<AsciiString>, rts::equal_to<AsciiString>/**/> AudioEventInfoHash;
typedef AudioEventInfoHash::iterator AudioEventInfoHashIt;
typedef UnsignedInt AudioHandle;


// Defines
enum
{
	PROVIDER_ERROR = 0xFFFFFFFF
};

// Class AudioManager
/**
	The life of audio.

	When audio is requested to play, it is done so in the following manner:
	1) An AudioEventRTS is created on the stack.
	2) Its guts are copied from elsewhere (for instance, a ThingTemplate, or MiscAudio).
	3) It is added to TheAudio via TheAudio->addAudioEvent(...)

	The return value from addAudioEvent can be saved in case the sound needs to loop and/or be
	terminated at some point.

	To reomve a playing sound, the call TheAudio->removeAudioEvent(...) is used. This will search
	the list of currently playing audio for the specified handle, and kill the attached sound. It
	will play a decay sound, if one is specified.

	The important functions of TheAudio, are therefore
		GameAudio::addAudioEvent()
		GameAudio::removeAudioEvent()
	All other functions exist to support these two basic requirements.

	In addition to the fundamental requirements, the audio has a fairly complicated sound management
	scheme. If all units were always allowed to sound off, the sound engine would be overwhelmed and
	would sound awful. Therefore, when an audio event is requested, it goes through a series of
	checks to determine if it is near enough to the camera, if it should be heard based on shroud,
	local player affiliation, etc. (The entire list of checks is contained in shouldPlayLocally()).

	In addition, the world and unit audio are never allowed to exceed their footprint, as specified
	in the audio settings INI file. In order to accommodate this, the audio uses an audio cache. The
	audio cache will attempt to load a sample, assuming there is enough room. If there is not enough
	room, then it goes through and finds any samples that are lower priority, and kills them until
	enough room is present for the sample. If it cannot free enough room, nothing happens to the
	cache.

	Although the audio is multithreaded, most of the operations are performed such that the worst
	case scenario for thread miscommunication is that the main thread misses an event for one frame.
	One specific case of this is the status of playing audio. Because audio is playing
	asynchronously, it can complete at any time. When most audio completes, it sets a flag on the
	event noting that it has completed. During the next update (from the main thread), anything with
	that flag set is moved to the stopped list, and then is cleaned up. (Basically, the audio uses
	a push model for its multithreadedness, which doesn't require thread safety such as mutexes or
	semaphores).

	All in all, the best way to learn how the audio works is to track the lifetime of an event
	through the system. This will give a better understanding than all the documentation I could
	write.

	-jkmcd
	-December 2002
*/

class AudioManager : public SubsystemInterface
{
	public:
		typedef UnsignedInt MuteAudioReasonInt;

		enum MuteAudioReason CPP_11(: UnsignedInt)
		{
			MuteAudioReason_WindowFocus,

			MuteAudioReason_Count
		};

		static const char *const MuteAudioReasonNames[];

		AudioManager();
		virtual ~AudioManager() override;
#if defined(RTS_DEBUG)
		virtual void audioDebugDisplay(DebugDisplayInterface *dd, void *userData, FILE *fp = nullptr ) = 0;
#endif

		// From SubsystemInterface
		virtual void init() override;
		virtual void postProcessLoad() override;
		virtual void reset() override;
		virtual void update() override;

		// device dependent stop, pause and resume
		virtual void stopAudio( AudioAffect which ) = 0;
		virtual void pauseAudio( AudioAffect which ) = 0;
		virtual void resumeAudio( AudioAffect which ) = 0;
		virtual void pauseAmbient( Bool shouldPause ) = 0;

		void muteAudio( MuteAudioReason reason );
		void unmuteAudio( MuteAudioReason reason );

		// control for AudioEventsRTS
		virtual AudioHandle addAudioEvent( const AudioEventRTS *eventToAdd );	///< Add an audio event (event must be declared in an INI file)
		virtual void removeAudioEvent( AudioHandle audioEvent );	///< Remove an audio event, stop for instance.
		virtual void killAudioEventImmediately( AudioHandle audioEvent ) = 0;

		virtual Bool isValidAudioEvent( const AudioEventRTS *eventToCheck ) const;	///< validate that this piece of audio exists
		virtual Bool isValidAudioEvent( AudioEventRTS *eventToCheck ) const;	///< validate that this piece of audio exists

		// add tracks during INIification
		void addTrackName( const AsciiString& trackName );
		AsciiString nextTrackName(const AsciiString& currentTrack );
		AsciiString prevTrackName(const AsciiString& currentTrack );

		// changing music tracks
		virtual AsciiString nextMusicTrack() = 0;
		virtual AsciiString prevMusicTrack() = 0;
		virtual Bool isMusicPlaying() const = 0;
		virtual Bool hasMusicTrackCompleted( const AsciiString& trackName, Int numberOfTimes ) const = 0;

		virtual void setAudioEventEnabled( AsciiString eventToAffect, Bool enable );
		virtual void setAudioEventVolumeOverride( AsciiString eventToAffect, Real newVolume );
		virtual void removeAudioEvent( AsciiString eventToRemove );
		virtual void removeDisabledEvents();

		// Really meant for internal purposes only, but cannot be protected.
		virtual void getInfoForAudioEvent( const AudioEventRTS *eventToFindAndFill ) const;	// Note: m_eventInfo is Mutable, and so this function will overwrite it if found

		///< Return whether the current audio is playing or not.
		///< NOTE NOTE NOTE !!DO NOT USE THIS IN FOR GAMELOGIC PURPOSES!! NOTE NOTE NOTE
		virtual Bool isCurrentlyPlaying( AudioHandle handle );

		// Device Dependent open and close functions
		virtual void openDevice() = 0;
		virtual void closeDevice() = 0;
		virtual void *getDevice() = 0;

		// Device Dependent notification functions
		virtual void notifyOfAudioCompletion( UnsignedInt audioCompleted, UnsignedInt flags ) = 0;

		// Device Dependent enumerate providers functions. It is okay for there to be only 1 provider (Miles provides a maximum of 64.
		virtual UnsignedInt getProviderCount() const = 0;
		virtual AsciiString getProviderName( UnsignedInt providerNum ) const = 0;
		virtual UnsignedInt getProviderIndex( AsciiString providerName ) const = 0;
		virtual void selectProvider( UnsignedInt providerNdx ) = 0;
		virtual void unselectProvider() = 0;
		virtual UnsignedInt getSelectedProvider() const = 0;
		virtual void setSpeakerType( UnsignedInt speakerType ) = 0;
		virtual UnsignedInt getSpeakerType() = 0;

		virtual UnsignedInt translateSpeakerTypeToUnsignedInt( const AsciiString& speakerType );
		virtual AsciiString translateUnsignedIntToSpeakerType( UnsignedInt speakerType );

		// Device Dependent calls to get the number of channels for each type of audio (2-D, 3-D, Streams)
		virtual UnsignedInt getNum2DSamples() const = 0;
		virtual UnsignedInt getNum3DSamples() const = 0;
		virtual UnsignedInt getNumStreams() const = 0;
		virtual UnsignedInt getNumAvailable2DSamples() const = 0;
		virtual UnsignedInt getNumAvailable3DSamples() const = 0;

		// Device Dependent calls to determine sound prioritization info
		virtual Bool doesViolateLimit( AudioEventRTS *event ) const = 0;
		virtual Bool isPlayingLowerPriority( AudioEventRTS *event ) const = 0;
		virtual Bool isPlayingAlready( AudioEventRTS *event ) const = 0;
		virtual Bool isObjectPlayingVoice( UnsignedInt objID ) const = 0;

		virtual void adjustVolumeOfPlayingAudio(AsciiString eventName, Real newVolume) = 0;
		virtual void removePlayingAudio( AsciiString eventName ) = 0;
		virtual void removeAllDisabledAudio() = 0;

		// Is the audio device on? We can skip a lot of audio processing if not.
		virtual Bool isOn( AudioAffect whichToGet ) const;
		virtual void setOn( Bool turnOn, AudioAffect whichToAffect );

		// Set and get the device Volume
		virtual void setVolume( Real volume, AudioAffect whichToAffect );
		virtual Real getVolume( AudioAffect whichToGet );

		// To get a more 3-D feeling from the universe, we adjust the volume of the 3-D samples based
		// on zoom.
		virtual void set3DVolumeAdjustment( Real volumeAdjustment );

    virtual Bool has3DSensitiveStreamsPlaying() const = 0;

 		virtual void *getHandleForBink() = 0;
 		virtual void releaseHandleForBink() = 0;

		// this function will play an audio event rts by loading it into memory. It should not be used
		// by anything except for the load screens.
		virtual void friend_forcePlayAudioEventRTS(const AudioEventRTS* eventToPlay) = 0;

		// Update Listener position information
		virtual void setListenerPosition( const Coord3D *newListenerPos, const Coord3D *newListenerOrientation );
		virtual const Coord3D *getListenerPosition() const;

		virtual AudioRequest *allocateAudioRequest( Bool useAudioEvent );
		virtual void releaseAudioRequest( AudioRequest *requestToRelease );
		virtual void appendAudioRequest( AudioRequest *m_request );
		virtual void processRequestList();

		virtual AudioEventInfo *newAudioEventInfo( AsciiString newEventName );
    virtual void addAudioEventInfo( AudioEventInfo * newEventInfo );
		virtual AudioEventInfo *findAudioEventInfo( AsciiString eventName ) const;

		const AudioSettings *getAudioSettings() const;
		const MiscAudio *getMiscAudio() const;

		// This function should only be called by AudioManager, MusicManager and SoundManager
		virtual void releaseAudioEventRTS( AudioEventRTS *&eventToRelease );

		// For INI
		AudioSettings *friend_getAudioSettings();
		MiscAudio *friend_getMiscAudio();
		const FieldParse *getFieldParseTable() const;

		const AudioEventRTS *getValidSilentAudioEvent() const { return m_silentAudioEvent; }

		virtual void setHardwareAccelerated(Bool accel) { m_hardwareAccel = accel; }
		virtual Bool getHardwareAccelerated() { return m_hardwareAccel; }

		virtual void setSpeakerSurround(Bool surround) { m_surroundSpeakers = surround; }
		virtual Bool getSpeakerSurround() { return m_surroundSpeakers; }

		virtual void refreshCachedVariables();

		virtual void setPreferredProvider(AsciiString providerNdx) = 0;
		virtual void setPreferredSpeaker(AsciiString speakerType) = 0;

		// For Scripting
		virtual Real getAudioLengthMS( const AudioEventRTS *event );
		virtual Real getFileLengthMS( AsciiString strToLoad ) const = 0;

		// For the file cache to know when to remove files.
		virtual void closeAnySamplesUsingFile( const void *fileToClose ) = 0;

		virtual Bool isMusicAlreadyLoaded() const;

		Bool getDisallowSpeech() const { return m_disallowSpeech; }
		void setDisallowSpeech( Bool disallowSpeech ) { m_disallowSpeech = disallowSpeech; }

		// For Worldbuilder, to build lists from which to select
		virtual void findAllAudioEventsOfType( AudioType audioType, std::vector<AudioEventInfo*>& allEvents );
    virtual const AudioEventInfoHash & getAllAudioEvents() const { return m_allAudioEventInfo; }

		Real getZoomVolume() const { return m_zoomVolume; }
	protected:

		// Is the currently selected provider actually HW accelerated?
		virtual Bool isCurrentProviderHardwareAccelerated();

		// Is the currently selected speaker type Surround sound?
		virtual Bool isCurrentSpeakerTypeSurroundSound();

		// Should this piece of audio play on the local machine?
		virtual Bool shouldPlayLocally(const AudioEventRTS *audioEvent);

		// Set the Listening position for the device
		virtual void setDeviceListenerPosition() = 0;

		// For tracking purposes
		virtual AudioHandle allocateNewHandle();

    // Remove all AudioEventInfo's with the m_isLevelSpecific flag
    virtual void removeLevelSpecificAudioEventInfos();

    void removeAllAudioRequests();

	protected:
		AudioSettings *m_audioSettings;
		MiscAudio *m_miscAudio;
		MusicManager *m_music;
		SoundManager *m_sound;
		Coord3D m_listenerPosition;
		Coord3D m_listenerOrientation;
		std::list<AudioRequest*> m_audioRequests;
		std::vector<AsciiString> m_musicTracks;

		AudioEventInfoHash m_allAudioEventInfo;
		AudioHandle theAudioHandlePool;
		std::list<std::pair<AsciiString, Real>/**/> m_adjustedVolumes;

		Real m_musicVolume;
		Real m_soundVolume;
		Real m_sound3DVolume;
		Real m_speechVolume;

		Real m_scriptMusicVolume;
		Real m_scriptSoundVolume;
		Real m_scriptSound3DVolume;
		Real m_scriptSpeechVolume;

		Real m_systemMusicVolume;
		Real m_systemSoundVolume;
		Real m_systemSound3DVolume;
		Real m_systemSpeechVolume;
		Real m_zoomVolume;


		AudioEventRTS *m_silentAudioEvent;

		enum
		{
			VOLUME_TYPE_MUSIC,
			VOLUME_TYPE_SOUND,
			VOLUME_TYPE_SOUND3D,
			VOLUME_TYPE_SPEECH,
			NUM_VOLUME_TYPES
		};
		Real *m_savedValues;
		MuteAudioReasonInt m_muteReasonBits;

		// Group of 8
		Bool m_speechOn						: 1;
		Bool m_soundOn						: 1;
		Bool m_sound3DOn					: 1;
		Bool m_musicOn						: 1;
		Bool m_volumeHasChanged		: 1;
		Bool m_hardwareAccel			: 1;
		Bool m_surroundSpeakers		: 1;
		Bool m_disallowSpeech			: 1;
};

extern AudioManager *TheAudio;
