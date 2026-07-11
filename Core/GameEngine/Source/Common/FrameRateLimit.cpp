/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

#include "PreRTS.h"
#include "Common/FrameRateLimit.h"

// GeneralsX @build BenderAI 12/02/2026 Platform-specific high-resolution timing
#ifndef _WIN32
#include <time.h>    // clock_gettime, nanosleep
#include <unistd.h>  // usleep (fallback)
#endif

FrameRateLimit::FrameRateLimit()
{
#ifdef _WIN32
	LARGE_INTEGER freq;
	LARGE_INTEGER start;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
	m_freq = freq.QuadPart;
	m_start = start.QuadPart;
#else
	// Linux: Use CLOCK_MONOTONIC for high-resolution timing
	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC, &start);
	m_freq = 1000000000; // nanoseconds per second
	m_start = static_cast<Int64>(start.tv_sec) * 1000000000 + start.tv_nsec;
#endif
}

Real FrameRateLimit::wait(UnsignedInt maxFps)
{
	PROFILER_SECTION;

	// GeneralsX @bugfix BenderAI 11/05/2026 Validate FPS limit to prevent division by zero and underflow
	// Skip limiting if maxFps is 0 or extremely high (uncapped mode)
	if (maxFps == 0 || maxFps >= 1000000)
	{
		// Uncapped or invalid: just return elapsed time without limiting
#ifdef _WIN32
		LARGE_INTEGER tick;
		QueryPerformanceCounter(&tick);
		double elapsedSeconds = static_cast<double>(tick.QuadPart - m_start) / m_freq;
		m_start = tick.QuadPart;
		return (Real)elapsedSeconds;
#else
		struct timespec tick;
		clock_gettime(CLOCK_MONOTONIC, &tick);
		Int64 tickValue = static_cast<Int64>(tick.tv_sec) * 1000000000 + tick.tv_nsec;
		double elapsedSeconds = static_cast<double>(tickValue - m_start) / static_cast<double>(m_freq);
		m_start = tickValue;
		return static_cast<Real>(elapsedSeconds);
#endif
	}

#ifdef _WIN32
	LARGE_INTEGER tick;
	QueryPerformanceCounter(&tick);
	double elapsedSeconds = static_cast<double>(tick.QuadPart - m_start) / m_freq;
	const double targetSeconds = 1.0 / maxFps;
	const double sleepSeconds = targetSeconds - elapsedSeconds - 0.002; // leave ~2ms for spin wait

	if (sleepSeconds > 0.0)
	{
		// Non busy wait with Munkee sleep
		DWORD dwMilliseconds = static_cast<DWORD>(sleepSeconds * 1000);
		Sleep(dwMilliseconds);
	}

	// Busy wait for remaining time
	do
	{
		QueryPerformanceCounter(&tick);
		elapsedSeconds = static_cast<double>(tick.QuadPart - m_start) / m_freq;
	}
	while (elapsedSeconds < targetSeconds);

	m_start = tick.QuadPart;
	return (Real)elapsedSeconds;
#else
	// Linux: Use CLOCK_MONOTONIC for high-resolution timing
	struct timespec tick;
	clock_gettime(CLOCK_MONOTONIC, &tick);
	Int64 tickValue = static_cast<Int64>(tick.tv_sec) * 1000000000 + tick.tv_nsec;
	double elapsedSeconds = static_cast<double>(tickValue - m_start) / static_cast<double>(m_freq);
	const double targetSeconds = 1.0 / maxFps;
	const double sleepSeconds = targetSeconds - elapsedSeconds - 0.002; // leave ~2ms for spin wait

	if (sleepSeconds > 0.0)
	{
		// Non busy wait with nanosleep
		struct timespec sleepTime;
		sleepTime.tv_sec = static_cast<time_t>(sleepSeconds);
		sleepTime.tv_nsec = static_cast<long>((sleepSeconds - sleepTime.tv_sec) * 1000000000);
		nanosleep(&sleepTime, nullptr);
	}

	// Busy wait for remaining time
	do
	{
		clock_gettime(CLOCK_MONOTONIC, &tick);
		tickValue = static_cast<Int64>(tick.tv_sec) * 1000000000 + tick.tv_nsec;
		elapsedSeconds = static_cast<double>(tickValue - m_start) / static_cast<double>(m_freq);
	}
	while (elapsedSeconds < targetSeconds);

	m_start = tickValue;
	return static_cast<Real>(elapsedSeconds);
#endif
}


const UnsignedInt RenderFpsPreset::s_fpsValues[] = {
	30, 50, 56, 60, 65, 70, 72, 75, 80, 85, 90, 100, 110, 120, 144, 240, 480, UncappedFpsValue };

static_assert(LOGICFRAMES_PER_SECOND <= 30, "Min FPS values need to be revisited!");

UnsignedInt RenderFpsPreset::getNextFpsValue(UnsignedInt value)
{
	const Int first = 0;
	const Int last = ARRAY_SIZE(s_fpsValues) - 1;
	for (Int i = first; i < last; ++i)
	{
		if (value >= s_fpsValues[i] && value < s_fpsValues[i + 1])
		{
			return s_fpsValues[i + 1];
		}
	}
	return s_fpsValues[last];
}

UnsignedInt RenderFpsPreset::getPrevFpsValue(UnsignedInt value)
{
	const Int first = 0;
	const Int last = ARRAY_SIZE(s_fpsValues) - 1;
	for (Int i = last; i > first; --i)
	{
		if (value <= s_fpsValues[i] && value > s_fpsValues[i - 1])
		{
			return s_fpsValues[i - 1];
		}
	}
	return s_fpsValues[first];
}

UnsignedInt RenderFpsPreset::changeFpsValue(UnsignedInt value, FpsValueChange change)
{
	switch (change)
	{
	default:
	case FpsValueChange_Increase: return getNextFpsValue(value);
	case FpsValueChange_Decrease: return getPrevFpsValue(value);
	}
}


UnsignedInt LogicTimeScaleFpsPreset::getNextFpsValue(UnsignedInt value)
{
	return value + StepFpsValue;
}

UnsignedInt LogicTimeScaleFpsPreset::getPrevFpsValue(UnsignedInt value)
{
	if (value - StepFpsValue < MinFpsValue)
	{
		return MinFpsValue;
	}
	else
	{
		return value - StepFpsValue;
	}
}

UnsignedInt LogicTimeScaleFpsPreset::changeFpsValue(UnsignedInt value, FpsValueChange change)
{
	switch (change)
	{
	default:
	case FpsValueChange_Increase: return getNextFpsValue(value);
	case FpsValueChange_Decrease: return getPrevFpsValue(value);
	}
}
