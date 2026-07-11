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
//                Copyright(C) 2001 - All Rights Reserved
//
//----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: GameSounds.cpp
//
// Created:   5/02/01
//
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
//         Includes
//----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Lib/BaseType.h"
#include "Common/GameSounds.h"

#include "Common/AudioEventInfo.h"
#include "Common/AudioEventRTS.h"
#include "Common/AudioRequest.h"
#include "Common/GameUtility.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"

#include "GameLogic/PartitionManager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
SoundManager::SoundManager()
{
	// nada to do
}

//-------------------------------------------------------------------------------------------------
SoundManager::~SoundManager()
{
	// nada to do
}

//-------------------------------------------------------------------------------------------------
void SoundManager::init()
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::postProcessLoad()
{
	// The AudioManager should actually be live now, so go ahead and get the info we need from it
	// here
}

//-------------------------------------------------------------------------------------------------
void SoundManager::update()
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::reset()
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::loseFocus()
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::regainFocus()
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::setListenerPosition( const Coord3D *position )
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::setViewRadius( Real viewRadius )
{

}

//-------------------------------------------------------------------------------------------------
void SoundManager::setCameraAudibleDistance( Real audibleDistance )
{

}

//-------------------------------------------------------------------------------------------------
Real SoundManager::getCameraAudibleDistance()
{
	return 1.0f;
}

//-------------------------------------------------------------------------------------------------
void SoundManager::addAudioEvent(AudioEventRTS *&eventToAdd)
{
	if (canPlayNow(eventToAdd)) {
#ifdef INTENSIVE_AUDIO_DEBUG
		DEBUG_LOG((" - appended to request list with handle '%d'.", (UnsignedInt) eventToAdd->getPlayingHandle()));
#endif
		AudioRequest *audioRequest = TheAudio->allocateAudioRequest( true );
		audioRequest->m_pendingEvent = eventToAdd;
		audioRequest->m_request = AR_Play;
		TheAudio->appendAudioRequest(audioRequest);
	} else {
		TheAudio->releaseAudioEventRTS(eventToAdd);
	}
}

//-------------------------------------------------------------------------------------------------
AsciiString SoundManager::getFilenameForPlayFromAudioEvent( const AudioEventRTS *eventToGetFrom )
{
	return AsciiString::TheEmptyString;
}

//-------------------------------------------------------------------------------------------------
Bool SoundManager::canPlayNow( AudioEventRTS *event )
{
	Bool retVal = false;
	// 1) Are we muted because we're beyond our maximum distance?
	// 2) Are we shrouded and this is a shroud sound?
	// 3) Are we violating our voice count or are we playing above the limit? (If so, stop now)
	// 4) is there an available channel open?
	// 5) if not, then determine if there is anything of lower priority that we can kill
	// 6) if not, are we an interrupt-sound type?
	// if so, are there any sounds of our type playing right now that we can interrupt?
	// potentially here: Are there any sounds that are playing that are now beyond their distance?
	// if so, kill them and start our sound
	// if not, we're done. Can't play dude.

	if( event->isPositionalAudio() && !BitIsSet( event->getAudioEventInfo()->m_type, ST_GLOBAL) && event->getAudioEventInfo()->m_priority != AP_CRITICAL )
	{
		Coord3D distance = *TheAudio->getListenerPosition();
		const Coord3D *pos = event->getCurrentPosition();
		if (pos)
		{
			distance.sub(*pos);
			if (distance.length() >= event->getAudioEventInfo()->m_maxDistance)
			{
#ifdef INTENSIVE_AUDIO_DEBUG
				DEBUG_LOG(("- culled due to distance (%.2f).", distance.length()));
#endif
				return false;
			}

			const Int localPlayerIndex = rts::getObservedOrLocalPlayer()->getPlayerIndex();

			if( (event->getAudioEventInfo()->m_type & ST_SHROUDED) &&
					 ThePartitionManager->getShroudStatusForPlayer(localPlayerIndex, pos) != CELLSHROUD_CLEAR )
			{
#ifdef INTENSIVE_AUDIO_DEBUG
				DEBUG_LOG(("- culled due to shroud."));
#endif
				return false;
			}
		}
	}

	if (violatesVoice(event))
	{
		retVal = isInterrupting(event);
		if (retVal)
		{
			return true;
		}
		else
		{
#ifdef INTENSIVE_AUDIO_DEBUG
		DEBUG_LOG(("- culled due to voice."));
#endif
			return false;
		}
	}

	if( TheAudio->doesViolateLimit( event ) )
	{
#ifdef INTENSIVE_AUDIO_DEBUG
		DEBUG_LOG(("- culled due to limit." ));
#endif
		return false;
	}
	else if( isInterrupting( event ) )
	{
		return true;
	}

	if (event->isPositionalAudio())
	{
		if (TheAudio->getNumAvailable3DSamples() > 0)
		{
			return true;
		}
#ifdef INTENSIVE_AUDIO_DEBUG
		DEBUG_LOG(("- %d samples playing, %d samples available",
			TheAudio->getNum3DSamples() - TheAudio->getNumAvailable3DSamples(), TheAudio->getNum3DSamples()));
#endif
	}
	else
	{
		// its a UI sound (and thus, 2-D)
		if (TheAudio->getNumAvailable2DSamples() > 0)
		{
			return true;
		}
	}

	if (TheAudio->isPlayingLowerPriority(event))
	{
		return true;
	}

	if (isInterrupting(event))
	{
		retVal = TheAudio->isPlayingAlready(event);
		if (retVal)
		{
			return true;
		}
		else
		{
#ifdef INTENSIVE_AUDIO_DEBUG
			DEBUG_LOG(("- culled due to no channels available and non-interrupting." ));
#endif
			return false;
		}
	}
#ifdef INTENSIVE_AUDIO_DEBUG
	DEBUG_LOG(("culled due to unavailable channels"));
#endif
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool SoundManager::violatesVoice( AudioEventRTS *event )
{
	if (event->getAudioEventInfo()->m_type & ST_VOICE) {
		return (event->getObjectID() && TheAudio->isObjectPlayingVoice(event->getObjectID()));
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool SoundManager::isInterrupting( AudioEventRTS *event )
{
	return event->getAudioEventInfo()->m_control & AC_INTERRUPT;
}
