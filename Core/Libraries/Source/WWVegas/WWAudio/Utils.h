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
 *                     $Archive:: /Commando/Code/WWAudio/Utils.h          $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 8/24/01 4:37p                                               $*
 *                                                                                             *
 *                    $Revision:: 4                                                           $*
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

/////////////////////////////////////////////////////////////////////////////
//
// Macros
//
#define SAFE_DELETE(pobject) { delete pobject; pobject = nullptr; }

#define SAFE_DELETE_ARRAY(pobject) { delete [] pobject; pobject = nullptr; }

#define SAFE_FREE(pobject) { ::free (pobject); pobject = nullptr; }


/////////////////////////////////////////////////////////////////////////////
//
//	MMSLockClass
//
/////////////////////////////////////////////////////////////////////////////
class MMSLockClass
{
	public:
		MMSLockClass () { ::AIL_lock (); }
		~MMSLockClass () { ::AIL_unlock (); }


	static CRITICAL_SECTION _MSSLockCriticalSection;
};


////////////////////////////////////////////////////////////////////////////
//
//  Get_Filename_From_Path
//
__inline LPCTSTR
Get_Filename_From_Path (LPCTSTR path)
{
	// Find the last occurrence of the directory deliminator
	LPCTSTR filename = ::strrchr (path, '\\');
	if (filename != nullptr) {
		// Increment past the directory deliminator
		filename ++;
	} else {
		filename = path;
	}

	// Return the filename part of the path
	return filename;
}
