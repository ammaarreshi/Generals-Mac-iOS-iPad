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
// File name:  Common/GameSounds.h
//
// Created:    5/02/01
//
//----------------------------------------------------------------------------

#pragma once

#include "Common/SubsystemInterface.h"
#include "Common/GameAudio.h"
#include "Common/GameType.h"

// Forward declarations
class AudioEventRTS;

class SoundManager : public SubsystemInterface
{
	public:
		SoundManager();
		virtual ~SoundManager() override;

		virtual void init() override;										///< Initializes the sounds system
		virtual void postProcessLoad() override;
		virtual void update() override;									///< Services sounds tasks. Called by AudioInterface
		virtual void reset() override;										///< Reset the sounds system

		virtual void loseFocus();								///< Called when application loses focus
		virtual void regainFocus();							///< Called when application regains focus

		virtual void setListenerPosition( const Coord3D *position );	///< Set the listener position for map3DSound() calculations
		virtual void setViewRadius( Real viewRadius );///< Sets the radius of the view from the center of the screen in world coordinate units
		virtual void setCameraAudibleDistance( Real audibleDistance );
		virtual Real getCameraAudibleDistance();

		virtual void addAudioEvent(AudioEventRTS *&eventToAdd);	// pre-copied

		// empty string means that this sound wasn't found or some error occurred. CHECK FOR EMPTY STRING.
		virtual AsciiString getFilenameForPlayFromAudioEvent( const AudioEventRTS *eventToGetFrom );

		// called by this class and MilesAudioManager to determine if a sound can still be played
		virtual Bool canPlayNow( AudioEventRTS *event );

	protected:
		virtual Bool violatesVoice( AudioEventRTS *event );
		virtual Bool isInterrupting( AudioEventRTS *event );
};
