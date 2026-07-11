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

// FILE: PerfTimer.cpp ///////////////////////////////////////////////////////////////////////////
// Author:
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/PerfTimer.h"

#include "Common/GlobalData.h"
#include "GameClient/DebugDisplay.h"
#include "GameClient/Display.h"
#include "GameClient/GraphDraw.h"
#include <cstdint>

// TheSuperHackers @build fighter19 11/02/2026 Cross-platform types (__int64 → int64_t, __forceinline → inline)
#ifdef _WIN32
__forceinline void ProfileGetTime(__int64 &t)
#else
inline void ProfileGetTime(int64_t &t)
#endif
{
	t = _rdtsc();
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

#if defined(PERF_TIMERS) || defined(DUMP_PERF_STATS)

//-------------------------------------------------------------------------------------------------
static Int64 s_ticksPerSec = 0;
static double s_ticksPerMSec = 0;
static double s_ticksPerUSec = 0;

//-------------------------------------------------------------------------------------------------
void GetPrecisionTimerTicksPerSec(Int64* t)
{
	*t = s_ticksPerSec;
}

//Kris: Plugged in Martin's code to optimize timer setup.
#define HOFFESOMMER_REPLACEMENT_CODE

//-------------------------------------------------------------------------------------------------
void InitPrecisionTimer()
{
#ifdef HOFFESOMMER_REPLACEMENT_CODE

  // measure clock cycles 3 times for 20 msec each
  // then take the 2 counts that are closest, average
  _int64 n[ 3 ];
  for( int k = 0; k < 3; k++ )
  {
    // wait for end of current tick
    unsigned timeEnd = timeGetTime() + 2;
    while( timeGetTime() < timeEnd ); //do nothing

    // get cycles
    _int64 start, startQPC, endQPC;
    QueryPerformanceCounter( (LARGE_INTEGER *)&startQPC );
    ProfileGetTime( start );
    timeEnd += 20;
    while( timeGetTime() < timeEnd ); //do nothing
    ProfileGetTime( n[ k ] );
    n[ k ] -= start;

    // convert to 1 second
    if( QueryPerformanceCounter( (LARGE_INTEGER*)&endQPC ) )
    {
      QueryPerformanceFrequency( (LARGE_INTEGER*)&s_ticksPerSec );
      n[ k ] = ( n[ k ] * s_ticksPerSec ) / ( endQPC - startQPC );
    }
    else
    {
      n[ k ] = ( n[ k ] * 1000 ) / 20;
    }
  }

  // find two closest values
  _int64 d01 = n[ 1 ] - n[ 0 ];
	_int64 d02 = n[ 2 ] - n[ 0 ];
	_int64 d12 = n[ 2 ] - n[ 1 ];

  if( d01 < 0 )
	{
		d01 = -d01;
	}
  if( d02 < 0 )
	{
		d02 = -d02;
	}
  if( d12 < 0 )
	{
		d12 = -d12;
	}

  _int64 avg;
  if( d01 < d02 )
  {
    avg = d01 < d12 ? n[ 0 ] + n[ 1 ] : n[ 1 ] + n[ 2 ];
  }
  else
  {
    avg = d02 < d12 ? n[ 0 ] + n[ 2 ] : n[ 1 ] + n[ 2 ];
  }

	//s_ticksPerMSec = 1.0 * TotalTicks / totalTime;
	s_ticksPerMSec = avg / 2000.0f;
	s_ticksPerSec = s_ticksPerMSec * 1000.0f;
	s_ticksPerUSec = s_ticksPerSec / 1000000.0f;


#else

	//Kris: With total disrespect, this code costs 5 real seconds of init time
	//whenever we fire up the game.

	#ifdef USE_QPF
		QueryPerformanceFrequency((LARGE_INTEGER*)&s_ticksPerSec);
	#else
		// Init the precision timers
		Int64 totalTime = 0;
		Int64	TotalTicks = 0;
		static int TESTS = 5;

		for (int i = 0; i < TESTS; ++i)
		{
			int        TimeStart;
			int        TimeStop;
			Int64		   StartTicks;
			Int64		   EndTicks;

			TimeStart = timeGetTime();
			GetPrecisionTimer(&StartTicks);
			for(;;)
			{
				TimeStop = timeGetTime();
				if ((TimeStop - TimeStart) > 1000)
				{
					GetPrecisionTimer(&EndTicks);
					break;
				}
			}

			TotalTicks += (EndTicks - StartTicks);

			totalTime += (TimeStop - TimeStart);
		}

		s_ticksPerMSec = 1.0 * TotalTicks / totalTime;
		s_ticksPerSec = s_ticksPerMSec * 1000.0f;
	#endif
		s_ticksPerMSec = s_ticksPerSec / 1000.0f;
		s_ticksPerUSec = s_ticksPerSec / 1000000.0f;

	#ifdef NOT_IN_USE
		Int64 bogus[8];
		GetPrecisionTimer(&start);
		for (Int ii = 0; ii < ITERS; ++ii)
		{
			GetPrecisionTimer(&bogus[0]);
			GetPrecisionTimer(&bogus[1]);
			GetPrecisionTimer(&bogus[2]);
			GetPrecisionTimer(&bogus[3]);
			GetPrecisionTimer(&bogus[4]);
			GetPrecisionTimer(&bogus[5]);
			GetPrecisionTimer(&bogus[6]);
			GetPrecisionTimer(&bogus[7]);
		}
		TheTicksToGetTicks = (bogus[7] - start) / (ITERS*8);
		DEBUG_LOG(("TheTicksToGetTicks is %d (%f usec)",(int)TheTicksToGetTicks,TheTicksToGetTicks/s_ticksPerUSec));
	#endif

#endif

}
#endif // defined(PERF_TIMERS) || defined(DUMP_PERF_STATS)

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

#ifdef PERF_TIMERS

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

/*static*/ Bool AutoPerfGatherIgnore::s_ignoring = false;

//-------------------------------------------------------------------------------------------------
typedef std::vector< std::pair< AsciiString, AsciiString >/**/> StringPairVec;

//-------------------------------------------------------------------------------------------------
// PerfMetrics class. Basically, request a handle with your name and it will return. We use a vector
// of pairs of asciistrings for this

class PerfMetricsOutput
{
private:
	StringPairVec m_outputStats;

public:

	AsciiString& getStatsString(const AsciiString& id)
	{
		for (int i = 0; i < m_outputStats.size(); ++i)
		{
			if (m_outputStats[i].first == id)
				return m_outputStats[i].second;
		}
		std::pair<AsciiString, AsciiString> newPair;
		newPair.first = id;
		m_outputStats.push_back(newPair);
		return m_outputStats.back().second;
	}

	void clearStatsString(const AsciiString& id)
	{
		for (int i = 0; i < m_outputStats.size(); ++i)
		{
			if (m_outputStats[i].first == id)
			{
				m_outputStats.erase(&m_outputStats[i]);
				return;
			}
		}
	}

	StringPairVec& friend_getAllStatsStrings() { return m_outputStats; }
};

//-------------------------------------------------------------------------------------------------
static PerfMetricsOutput s_output;
static FILE* s_perfStatsFile = nullptr;
static Int s_perfDumpOptions = 0;
static UnsignedInt s_lastDumpedFrame = 0;
static char s_buf[256] = "";

PerfGather*		PerfGather::m_active[MAX_ACTIVE_STACK] = { 0 };
PerfGather**	PerfGather::m_activeHead = &PerfGather::m_active[0];
Int64					PerfGather::s_stopStartOverhead = -1;


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
/*static*/ PerfGather*& PerfGather::getHeadPtr()
{
	// funky technique for order-of-init problem. trust me. (srj)
	static PerfGather* s_head = nullptr;
	return s_head;
}

//-------------------------------------------------------------------------------------------------
void PerfGather::addToList()
{
	PerfGather*& head = getHeadPtr();

	this->m_next = head;
	if (head)
		head->m_prev = this;
	head = this;
}

//-------------------------------------------------------------------------------------------------
void PerfGather::removeFromList()
{
	PerfGather*& head = getHeadPtr();

	if (this->m_next)
		this->m_next->m_prev = this->m_prev;

	if (this->m_prev)
		this->m_prev->m_next = this->m_next;
	else
		head = this->m_next;

	this->m_prev = 0;
	this->m_next = 0;
}

//-------------------------------------------------------------------------------------------------
PerfGather::PerfGather(const char *identifier) :
	m_identifier(identifier),
	m_startTime(0),
	m_runningTimeGross(0),
	m_runningTimeNet(0),
	m_callCount(0),
	m_next(0),
	m_prev(0)
{
	m_ignore = FALSE;
	DEBUG_ASSERTCRASH(strchr(m_identifier, ',') == nullptr, ("PerfGather names must not contain commas"));
	addToList();
}

//-------------------------------------------------------------------------------------------------
PerfGather::~PerfGather()
{
	removeFromList();
}

//-------------------------------------------------------------------------------------------------
void PerfGather::reset()
{
	m_startTime = 0;
	m_runningTimeGross = 0;
	m_runningTimeNet = 0;
	m_callCount = 0;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void PerfGather::resetAll()
{
	for (PerfGather* head = getHeadPtr(); head != nullptr; head = head->m_next)
	{
		head->reset();
	}
}

//-------------------------------------------------------------------------------------------------
/*static*/ void PerfGather::initPerfDump(const char* fname, Int options)
{
	PerfGather::termPerfDump();

	strlcpy(s_buf, fname, ARRAY_SIZE(s_buf);

	char tmp[256];
	static_assert(ARRAY_SIZE(tmp) >= ARRAY_SIZE(s_buf), "Incorrect array size");
	strcpy(tmp, s_buf);
	strlcat(tmp, ".csv", ARRAY_SIZE(tmp));

	s_perfStatsFile = fopen(tmp, "w");
	s_perfDumpOptions = options;

	if (s_perfStatsFile == nullptr)
	{
		DEBUG_CRASH(("could not open/create perf file %s -- is it open in another app?",s_buf));
		return;
	}

	if (s_stopStartOverhead == -1)
	{
		const Int ITERS = 100000;
		Int64 start, end;
		PerfGather pf("timer");
		GetPrecisionTimer(&start);
		for (Int ii = 0; ii < ITERS; ++ii)
		{
			pf.startTimer(); pf.stopTimer();
			pf.startTimer(); pf.stopTimer();
			pf.startTimer(); pf.stopTimer();
			pf.startTimer(); pf.stopTimer();
			pf.startTimer(); pf.stopTimer();
			pf.startTimer(); pf.stopTimer();
			pf.startTimer(); pf.stopTimer();
			pf.startTimer(); pf.stopTimer();
		}
		GetPrecisionTimer(&end);
		s_stopStartOverhead = (end - start) / (ITERS*8);
		DEBUG_LOG(("s_stopStartOverhead is %d (%f usec)",(int)s_stopStartOverhead,s_stopStartOverhead/s_ticksPerUSec));
	}
}

//-------------------------------------------------------------------------------------------------
/*static*/ void PerfGather::dumpAll(UnsignedInt frame)
{
	if (frame < s_lastDumpedFrame)
	{
		// must have reset or started a new game.
		termPerfDump();
		initPerfDump(s_buf, s_perfDumpOptions);
	}

	if (!s_perfStatsFile)
	{
		DEBUG_CRASH(("not inited"));
		return;
	}

	if (frame >= 1 && frame <= 30)
	{
		// always skip the first second or so, since it loads everything and skews the results horribly
	}
	else
	{
		if (s_lastDumpedFrame == 0)
		{
			fprintf(s_perfStatsFile, "Frame");
			if (s_perfDumpOptions & PERF_GROSSTIME)
			{
				for (const PerfGather* head = getHeadPtr(); head != nullptr; head = head->m_next)
				{
					fprintf(s_perfStatsFile, ",Gross:%s", head->m_identifier);
				}
			}
			if (s_perfDumpOptions & PERF_NETTIME)
			{
				for (const PerfGather* head = getHeadPtr(); head != nullptr; head = head->m_next)
				{
					fprintf(s_perfStatsFile, ",Net:%s", head->m_identifier);
				}
			}
			if (s_perfDumpOptions & PERF_CALLCOUNT)
			{
				for (const PerfGather* head = getHeadPtr(); head != nullptr; head = head->m_next)
				{
					fprintf(s_perfStatsFile, ",Count:%s", head->m_identifier);
				}
			}
			fprintf(s_perfStatsFile, "\n");
		}

		// a strange value so we can find it in the dump, if necessary.
		// there's nothing magic about this value, it's purely determined from sample dumps...
//		const Real CLIP_BIG_SPIKES = 1e10f;
		const Real CLIP_BIG_SPIKES = 100000.0f;

		// make this a nonnumeric thing so Excel won't try to graph it...
		fprintf(s_perfStatsFile, "Frame%08d", frame);
		if (s_perfDumpOptions & PERF_GROSSTIME)
		{
			for (const PerfGather* head = getHeadPtr(); head != nullptr; head = head->m_next)
			{
				double t = head->m_runningTimeGross;
				t /= s_ticksPerUSec;
				if (t > CLIP_BIG_SPIKES)
					t = CLIP_BIG_SPIKES;
				fprintf(s_perfStatsFile, ",%f", t);
			}
		}
		if (s_perfDumpOptions & PERF_NETTIME)
		{
			for (const PerfGather* head = getHeadPtr(); head != nullptr; head = head->m_next)
			{
				double t = head->m_runningTimeNet;
				t /= s_ticksPerUSec;
				if (t > CLIP_BIG_SPIKES)
					t = CLIP_BIG_SPIKES;
				fprintf(s_perfStatsFile, ",%f", t);
			}
		}
		if (s_perfDumpOptions & PERF_CALLCOUNT)
		{
			for (const PerfGather* head = getHeadPtr(); head != nullptr; head = head->m_next)
			{
				fprintf(s_perfStatsFile, ",%d", head->m_callCount);
			}
		}

		fprintf(s_perfStatsFile, "\n");
		fflush(s_perfStatsFile);

		s_lastDumpedFrame = frame;

	}

}

//-------------------------------------------------------------------------------------------------
// This function will queue up stuff to draw on the next frame. We also need to adjust the
// perf timers to not include time spent paused by the script engine.
/*static*/ void PerfGather::displayGraph(UnsignedInt frame)
{
	if (!TheGraphDraw) {
		return;
	}

	if (frame >= 1 && frame <= 30)
	{
		// always skip the first second or so, since it loads everything and skews the results horribly
	}
	else
	{
		const Real CLIP_BIG_SPIKES = 100000.0f;

		if (s_perfDumpOptions & PERF_GROSSTIME)
		{
			for (const PerfGather* head = getHeadPtr(); head != nullptr; head = head->m_next)
			{
				Real t = head->m_runningTimeGross;
				t /= s_ticksPerUSec;
				if (t > CLIP_BIG_SPIKES)
					t = CLIP_BIG_SPIKES;

				TheGraphDraw->addEntry(head->m_identifier, REAL_TO_INT(t));
			}
		}
		if (s_perfDumpOptions & PERF_NETTIME)
		{
			for (const PerfGather* head = getHeadPtr(); head != nullptr; head = head->m_next)
			{
				Real t = head->m_runningTimeNet;
				t /= s_ticksPerUSec;
				if (t > CLIP_BIG_SPIKES)
					t = CLIP_BIG_SPIKES;

				TheGraphDraw->addEntry(head->m_identifier, REAL_TO_INT(t));
			}
		}
		if (s_perfDumpOptions & PERF_CALLCOUNT)
		{
			for (const PerfGather* head = getHeadPtr(); head != nullptr; head = head->m_next)
			{
				Real t = head->m_callCount;
				TheGraphDraw->addEntry(head->m_identifier, REAL_TO_INT(t));

			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
/*static*/ void PerfGather::termPerfDump()
{
	if (s_perfStatsFile)
	{
		fflush(s_perfStatsFile);
		fclose(s_perfStatsFile);
		s_perfStatsFile = nullptr;
	}
	s_lastDumpedFrame = 0;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
PerfTimer::PerfTimer( const char *identifier, Bool crashWithInfo, Int startFrame, Int endFrame) :
	m_identifier(identifier),
	m_crashWithInfo(crashWithInfo),
	m_startFrame(startFrame),
	m_endFrame(endFrame),
	m_callCount(0),
	m_runningTime(0),
	m_outputInfo(true),
	m_lastFrame(-1)
{
}

//-------------------------------------------------------------------------------------------------
PerfTimer::~PerfTimer( )
{
	if (m_endFrame == -1) {
		outputInfo();
	}
}

//-------------------------------------------------------------------------------------------------
void PerfTimer::outputInfo()
{
	if (TheGlobalData->m_showMetrics) {
		return;
	}

	if (m_outputInfo && !TheGlobalData->m_showMetrics) {
		m_outputInfo = false;
	} else {
		return;
	}

	if (!s_ticksPerSec) {
		// DEBUG_CRASH here
		return;
	}

#if defined(RTS_DEBUG)
	double totalTimeInMS = 1000.0 * m_runningTime / s_ticksPerSec;
	double avgTimePerFrame = totalTimeInMS / (m_lastFrame - m_startFrame + 1);
	double avgTimePerCall = totalTimeInMS / m_callCount;
#endif

	if (m_crashWithInfo) {
		DEBUG_CRASH(("%s\n"
								 "Average Time (per call): %.4f ms\n"
								 "Average Time (per frame): %.4f ms\n"
								 "Average calls per frame: %.2f\n"
								 "Number of calls: %d\n"
								 "Max possible FPS: %.4f",
								 m_identifier,
								 avgTimePerCall,
								 avgTimePerFrame,
								 1.0f * m_callCount / (m_lastFrame - m_startFrame + 1),
								 m_callCount,
								 1000.0f / avgTimePerFrame));
	} else {
		DEBUG_LOG(("%s\n"
								 "Average Time (per call): %.4f ms\n"
								 "Average Time (per frame): %.4f ms\n"
								 "Average calls per frame: %.2f\n"
								 "Number of calls: %d\n"
								 "Max possible FPS: %.4f",
								 m_identifier,
								 avgTimePerCall,
								 avgTimePerFrame,
								 1.0f * m_callCount / (m_lastFrame - m_startFrame + 1),
								 m_callCount,
								 1000.0f / avgTimePerFrame));
	}
}

//-------------------------------------------------------------------------------------------------
void PerfTimer::showMetrics()
{
#if defined(RTS_DEBUG)
	double totalTimeInMS = 1000.0 * m_runningTime / s_ticksPerSec;
	double avgTimePerFrame = totalTimeInMS / (m_lastFrame - m_startFrame + 1);
	double avgTimePerCall = totalTimeInMS / m_callCount;
#endif

	// we want to work on the thing in the array, so just store a reference.
	AsciiString &outputStats = s_output.getStatsString(m_identifier);

	outputStats.format("%s: %.2fms / call, %.2fms / frame \n",
											m_identifier,
											avgTimePerCall,
											avgTimePerFrame);
	m_callCount = 0;
	m_runningTime = 0;

	UnsignedInt frm = (TheGameLogic ? TheGameLogic->getFrame() : m_startFrame);
	m_startFrame = frm + 1;
	m_endFrame = m_startFrame + PERFMETRICS_BETWEEN_METRICS;
}

//-------------------------------------------------------------------------------StatMetricsDisplay
void StatMetricsDisplay( DebugDisplayInterface *dd, void *, FILE *fp )
{
	dd->printf("Performance Metrics: \n");
	// no copies will take place because we are storing a reference to the thing
	StringPairVec &stats = s_output.friend_getAllStatsStrings();

	for (int i = 0; i < stats.size(); ++i) {
		dd->printf("%s", stats[i].second.str());
	}
}

#endif	// PERF_TIMERS

