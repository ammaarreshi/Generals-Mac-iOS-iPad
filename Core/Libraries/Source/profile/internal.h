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

/////////////////////////////////////////////////////////////////////////EA-V1
// $File: //depot/GeneralsMD/Staging/code/Libraries/Source/profile/internal.h $
// $Author: mhoffe $
// $Revision: #3 $
// $DateTime: 2003/07/09 10:57:23 $
//
// (c) 2003 Electronic Arts
//
// Internal header
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Utility/intrin_compat.h>

#include "../debug/debug.h"
#include "internal_funclevel.h"
#include "internal_highlevel.h"
#include "internal_cmd.h"
#include "internal_result.h"

#ifdef _WIN32
#include <windows.h>
#endif

#if !(defined(_MSC_VER) && _MSC_VER < 1300)
#include <atomic>
#endif

class ProfileFastCS
{
  ProfileFastCS(const ProfileFastCS&) CPP_11(= delete);
  ProfileFastCS& operator=(const ProfileFastCS&) CPP_11(= delete);

#ifdef _WIN32
	static HANDLE testEvent;
#endif

#if defined(_MSC_VER) && _MSC_VER < 1300
	volatile unsigned m_Flag;

	void ThreadSafeSetFlag()
	{
		volatile unsigned& nFlag=m_Flag;

		#define ts_lock _emit 0xF0
		DASSERT(((unsigned)&nFlag % 4) == 0);

		__asm {
			mov ebx, [nFlag]
			ts_lock
			bts dword ptr [ebx], 0
			jc The_Bit_Was_Previously_Set_So_Try_Again
		}
		return;

	The_Bit_Was_Previously_Set_So_Try_Again:
    // can't use SwitchToThread() here because Win9X doesn't have it!
    if (testEvent)
		  ::WaitForSingleObject(testEvent,1);
		__asm {
			mov ebx, [nFlag]
			ts_lock
			bts dword ptr [ebx], 0
			jc  The_Bit_Was_Previously_Set_So_Try_Again
		}
	}

	void ThreadSafeClearFlag()
	{
		m_Flag=0;
	}

public:
	ProfileFastCS():
    m_Flag(0)
  {
  }
#else

	std::atomic_flag Flag{};

	void ThreadSafeSetFlag()
	{
		while (Flag.test_and_set(std::memory_order_acq_rel)) {
			Flag.wait(true, std::memory_order_relaxed);
		}
	}

	void ThreadSafeClearFlag()
	{
		Flag.clear(std::memory_order_release);
		Flag.notify_one();
	}

public:
	ProfileFastCS() {}

#endif

	class Lock
	{
    Lock(const Lock&) CPP_11(= delete);
	Lock& operator=(const Lock&) CPP_11(= delete);

		ProfileFastCS& CriticalSection;

	public:
		Lock(ProfileFastCS& cs):
      CriticalSection(cs)
		{
			CriticalSection.ThreadSafeSetFlag();
		}

		~Lock()
		{
			CriticalSection.ThreadSafeClearFlag();
		}
	};

	friend class Lock;
};

void *ProfileAllocMemory(unsigned numBytes);
void *ProfileReAllocMemory(void *oldPtr, unsigned newSize);
void ProfileFreeMemory(void *ptr);

__forceinline void ProfileGetTime(__int64 &t)
{
#if defined(_MSC_VER) && _MSC_VER < 1300
  _asm
  {
    mov ecx,[t]
    push eax
    push edx
    rdtsc
    mov [ecx],eax
    mov [ecx+4],edx
    pop edx
    pop eax
  };
#else
  t = static_cast<__int64>(_rdtsc());
#endif
}
