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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WWAudio                                                      *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/WWAudio/SoundBuffer.h                        $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 8/17/01 11:12a                                              $*
 *                                                                                             *
 *                    $Revision:: 7                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

// TheSuperHackers @build 09/02/2026 Conditionally include Miles (Windows only)
// GeneralsX @feature fbraz 11/06/2026 Also use MilesStub when MiniAudio is enabled
#if !defined(SAGE_USE_OPENAL) && !defined(SAGE_USE_MINIAUDIO)
#pragma warning (push, 3)
#include "mss.h"
#pragma warning (pop)
#else
#include "MilesStub.h"
#endif

#include "always.h"


// Forward declarations
class FileClass;


/////////////////////////////////////////////////////////////////////////////////
//
//	SoundBufferClass
//
//	A sound buffer manages the raw sound data for any of the SoundObj types
// except for the StreamSoundClass object.
//
class SoundBufferClass : public RefCountClass
{
	public:

		//////////////////////////////////////////////////////////////////////
		//	Public constructors/destructors
		//////////////////////////////////////////////////////////////////////
		SoundBufferClass ();
		virtual ~SoundBufferClass () override;

		//////////////////////////////////////////////////////////////////////
		//	Public operators
		//////////////////////////////////////////////////////////////////////
		operator unsigned char * ()							{ return Get_Raw_Buffer (); }

		//////////////////////////////////////////////////////////////////////
		//	File methods
		//////////////////////////////////////////////////////////////////////
		virtual bool				Load_From_File (const char *filename);
		virtual bool				Load_From_File (FileClass &file);

		//////////////////////////////////////////////////////////////////////
		//	Memory methods
		//////////////////////////////////////////////////////////////////////
		virtual bool				Load_From_Memory (unsigned char *mem_buffer, unsigned long size);

		//////////////////////////////////////////////////////////////////////
		//	Buffer access
		//////////////////////////////////////////////////////////////////////
		virtual unsigned char *	Get_Raw_Buffer () const	{ return m_Buffer; }
		virtual unsigned long	Get_Raw_Length () const	{ return m_Length; }

		//////////////////////////////////////////////////////////////////////
		//	Information methods
		//////////////////////////////////////////////////////////////////////
		virtual const char *		Get_Filename () const		{ return m_Filename; }
		virtual void				Set_Filename (const char *name);
		virtual unsigned long	Get_Duration () const		{ return m_Duration; }
		virtual unsigned long	Get_Rate () const			{ return m_Rate; }
		virtual unsigned long	Get_Bits () const			{ return m_Bits; }
		virtual unsigned long	Get_Channels () const		{ return m_Channels; }
		virtual unsigned long	Get_Type () const			{ return m_Type; }

		//////////////////////////////////////////////////////////////////////
		//	Type methods
		//////////////////////////////////////////////////////////////////////
		virtual bool				Is_Streaming () const		{ return false; }

	protected:

		//////////////////////////////////////////////////////////////////////
		//	Protected methods
		//////////////////////////////////////////////////////////////////////
		virtual void			Free_Buffer ();
		virtual void			Determine_Stats (unsigned char *buffer);

		//////////////////////////////////////////////////////////////////////
		//	Protected member data
		//////////////////////////////////////////////////////////////////////
		unsigned char *		m_Buffer;
		unsigned long			m_Length;
		char *					m_Filename;
		unsigned long			m_Duration;
		unsigned long			m_Rate;
		unsigned long			m_Bits;
		unsigned long			m_Channels;
		unsigned long			m_Type;
};


/////////////////////////////////////////////////////////////////////////////////
//
//	StreamSoundBufferClass
//
//	A sound buffer manages the raw sound data for any of the SoundObj types
// except for the StreamSoundClass object.
//
class StreamSoundBufferClass : public SoundBufferClass
{
	public:

		//////////////////////////////////////////////////////////////////////
		//	Public constructors/destructors
		//////////////////////////////////////////////////////////////////////
		StreamSoundBufferClass ();
		virtual ~StreamSoundBufferClass () override;

		//////////////////////////////////////////////////////////////////////
		//	File methods
		//////////////////////////////////////////////////////////////////////
		virtual bool			Load_From_File (const char *filename) override;
		virtual bool			Load_From_File (FileClass &file) override;

		//////////////////////////////////////////////////////////////////////
		//	Memory methods
		//////////////////////////////////////////////////////////////////////
		virtual bool			Load_From_Memory (unsigned char *mem_buffer, unsigned long size) override { return false; }

		//////////////////////////////////////////////////////////////////////
		//	Type methods
		//////////////////////////////////////////////////////////////////////
		virtual bool			Is_Streaming () const override { return true; }

	protected:

		//////////////////////////////////////////////////////////////////////
		//	Protected methods
		//////////////////////////////////////////////////////////////////////
		virtual void			Free_Buffer () override;
		virtual bool			Load_From_File (HANDLE hfile, unsigned long size, unsigned long offset);

		//////////////////////////////////////////////////////////////////////
		//	Protected member data
		//////////////////////////////////////////////////////////////////////
};
