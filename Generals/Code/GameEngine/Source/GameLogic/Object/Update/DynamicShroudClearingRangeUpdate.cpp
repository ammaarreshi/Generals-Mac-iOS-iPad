/*
**	Command & Conquer Generals(tm)
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

// FILE: DynamicShroudClearingRangeUpdate.cpp /////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, August 2002
// Desc:   Changes the Objects Shroud Clearing range
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/DynamicShroudClearingRangeUpdate.h"





//-------------------------------------------------------------------------------------------------
DynamicShroudClearingRangeUpdateModuleData::DynamicShroudClearingRangeUpdateModuleData()
{
	m_shrinkDelay = 0;
	m_shrinkTime = 0;
	m_growDelay = 0;
	m_growTime = 0;

	m_finalVision = 0.0f;
	m_changeInterval = 0;
	m_growInterval = 0;
	m_doSpySatFX = FALSE;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void DynamicShroudClearingRangeUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  UpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "ChangeInterval",			INI::parseDurationUnsignedInt,	nullptr, offsetof( DynamicShroudClearingRangeUpdateModuleData, m_changeInterval ) },
		{ "GrowInterval",			INI::parseDurationUnsignedInt,	nullptr, offsetof( DynamicShroudClearingRangeUpdateModuleData, m_growInterval ) },
		{ "ShrinkDelay",				INI::parseDurationUnsignedInt,	nullptr, offsetof( DynamicShroudClearingRangeUpdateModuleData, m_shrinkDelay ) },
		{ "ShrinkTime",					INI::parseDurationUnsignedInt,	nullptr, offsetof( DynamicShroudClearingRangeUpdateModuleData, m_shrinkTime ) },
		{ "GrowDelay",					INI::parseDurationUnsignedInt,	nullptr, offsetof( DynamicShroudClearingRangeUpdateModuleData, m_growDelay ) },
		{ "GrowTime",						INI::parseDurationUnsignedInt,	nullptr, offsetof( DynamicShroudClearingRangeUpdateModuleData, m_growTime ) },
		{ "FinalVision",				INI::parseReal,									nullptr, offsetof( DynamicShroudClearingRangeUpdateModuleData, m_finalVision ) },
		{ "GridDecalTemplate", RadiusDecalTemplate::parseRadiusDecalTemplate, nullptr, offsetof( DynamicShroudClearingRangeUpdateModuleData, m_gridDecalTemplate ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
//
//  doneForeverFrame
//  o                |                                     shrinkdelay                    |
//  |<-----------------------------------stateCountDown---------------------------------->|
//  |  shrinktime    |                                     |   growtime  |   growdelay    |
//  |                shrinkStartDeadline                   sustain       growStartDeadline|
//
//-------------------------------------------------------------------------------------------------
DynamicShroudClearingRangeUpdate::DynamicShroudClearingRangeUpdate( Thing *thing, const ModuleData* moduleData ) : UpdateModule( thing, moduleData )
{
	DynamicShroudClearingRangeUpdateModuleData *md = (DynamicShroudClearingRangeUpdateModuleData *)moduleData;
//	m_shrinkStartCountdown = md->m_shrinkDelay;
//	m_shrinkDeadline = 0;
	m_changeIntervalCountdown = 0;
	m_visionChangePerInterval = 0.0f;


	m_stateCountDown = md->m_shrinkDelay + md->m_shrinkTime;// total time
	m_totalFrames = max(1,m_stateCountDown);
	m_shrinkStartDeadline = m_stateCountDown - md->m_shrinkDelay;
	m_growStartDeadline =   m_stateCountDown - md->m_growDelay;
	m_sustainDeadline  = m_growStartDeadline - md->m_growTime;

	DEBUG_ASSERTCRASH((m_sustainDeadline >= m_shrinkStartDeadline),("DynamicShroudClearingRangeUpdate\nYOU WILL NEVER REACH FULL SHROUD CLEARING RANGE WITH THESE TIMES AND DELAYS"));
	DEBUG_ASSERTCRASH((m_growStartDeadline >= m_shrinkStartDeadline),("DynamicShroudClearingRangeUpdate\nYOU WILL NEVER REACH FULL SHROUD CLEARING RANGE WITH THESE TIMES AND DELAYS"));

	m_doneForeverFrame = TheGameLogic->getFrame() + m_stateCountDown;// a failsafe to force a shutdown on schedule

//	m_shrinkStartDeadline;
//	m_doneForeverFrame; //

	m_decalsCreated = FALSE;
	m_nativeClearingRange = 200;// a sensible default
	m_currentClearingRange = 0;

	m_state = DSCRU_NOT_STARTED_YET;
//	m_shrinkFinished = FALSE;
//	m_shrinkStarted = FALSE;

	Object* obj = getObject();
	if ( ! obj )
		return;
	Player* controller = obj->getControllingPlayer();
	if ( ! controller )
		return;

	m_nativeClearingRange = obj->getShroudClearingRange();//capture this before we start monkeying with it

	for (int d = 0; d < GRID_FX_DECAL_COUNT; ++d)
	{
		m_gridDecal[d].clear();
	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
DynamicShroudClearingRangeUpdate::~DynamicShroudClearingRangeUpdate()
{
	killGridDecals();// just in case

}


//-------------------------------------------------------------------------------------------------
void DynamicShroudClearingRangeUpdate::createGridDecals( const RadiusDecalTemplate& tmpl, Real radius, const Coord3D& pos )
{
	for (int d = 0; d < GRID_FX_DECAL_COUNT; ++d)
	{
		m_gridDecal[d].clear();
		tmpl.createRadiusDecal(pos, radius, getObject()->getControllingPlayer(), m_gridDecal[d]);
		m_gridDecal[d].setPosition(pos);
	}


}


//-------------------------------------------------------------------------------------------------
void DynamicShroudClearingRangeUpdate::animateGridDecals()
{

	const Coord3D *ctr = getObject()->getPosition();
	Coord3D pos;
	pos.z = 0;

	Real radius = m_currentClearingRange + ((m_totalFrames - m_stateCountDown) * 2);
	Real angle = 0.0f;
	Real angleInc = (PI*2.0f) / (Real)GRID_FX_DECAL_COUNT;
	Real opacity = 1.0f - (m_currentClearingRange / m_nativeClearingRange);

	for (int d = 0; d < GRID_FX_DECAL_COUNT; ++d)
	{
		pos.x = ctr->x + (WWMath::Sin(angle) * radius);
		pos.y = ctr->y + (WWMath::Cos(angle) * radius);

		pos.x -= ((Int)pos.x)%23;
		pos.y -= ((Int)pos.y)%23;

		m_gridDecal[d].setPosition(pos);
		m_gridDecal[d].setOpacity( opacity );
		angle += angleInc;

	}


}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void DynamicShroudClearingRangeUpdate::killGridDecals()
{
	for (int d = 0; d < GRID_FX_DECAL_COUNT; ++d)
	{
		m_gridDecal[d].clear();
	}

}



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpdateSleepTime DynamicShroudClearingRangeUpdate::update()
{
	if (m_state == DSCRU_SLEEPING) {
		return UPDATE_SLEEP_NONE;
	}
	//Housekeeping-----------------------------------------
	Object *me = getObject();
	const DynamicShroudClearingRangeUpdateModuleData *md = getDynamicShroudClearingRangeUpdateModuleData();
	UnsignedInt currentFrame = TheGameLogic->getFrame();

	if ( ! m_decalsCreated )
	{
		createGridDecals(md->m_gridDecalTemplate, 100, *(me->getPosition()));
		m_decalsCreated = TRUE;
	}
	//-----------------------------------------------------


	if( m_stateCountDown <= 0 || currentFrame > m_doneForeverFrame )
		m_state = DSCRU_DONE_FOREVER;
	else if ( m_stateCountDown <= m_shrinkStartDeadline  )
		m_state = DSCRU_SHRINKING;
	else if ( m_stateCountDown <= m_sustainDeadline )
		m_state = DSCRU_SUSTAINING;
	else if ( m_stateCountDown <= m_growStartDeadline )
		m_state = DSCRU_GROWING;


	switch (m_state)
	{

		case DSCRU_NOT_STARTED_YET :
		{
			animateGridDecals();

			break;
		}
		case DSCRU_GROWING :
		{
			animateGridDecals();

			m_currentClearingRange += m_nativeClearingRange / max(1.0f, (Real)md->m_growTime);
			if (m_currentClearingRange >= m_nativeClearingRange)
				m_state = DSCRU_SUSTAINING;
			break;
		}
		case DSCRU_SUSTAINING :
		{
			m_currentClearingRange = m_nativeClearingRange;
			killGridDecals();
			break;
		}
		case DSCRU_SHRINKING :
		{
			m_currentClearingRange -= (m_nativeClearingRange-md->m_finalVision) / max(1.0f, (Real)md->m_shrinkTime);
			break;
		}
		case DSCRU_DONE_FOREVER :
		{
			killGridDecals();
			m_currentClearingRange = md->m_finalVision;
			break;
		}

	}

	if ( m_stateCountDown > 0 ) m_stateCountDown --;// it is important that this gets called every frame without sleeping
	//because it handles animation and may need to respond to changing vision range from scripts & stuff


	if( m_changeIntervalCountdown > 0 )
		m_changeIntervalCountdown--;
	else
	{// reset per change timer
		m_changeIntervalCountdown = ( m_state == DSCRU_GROWING ? md->m_growInterval : md->m_changeInterval);
		me->setShroudClearingRange( m_currentClearingRange );
		if (m_state == DSCRU_DONE_FOREVER) { // We are done forever, and have done the final update, so sleep.+
			m_state = DSCRU_SLEEPING;
		}
	}

	return UPDATE_SLEEP_NONE;



//	if( !m_shrinkStarted )
//	{
//		//To allow a start delay of zero, check before dec, and look for trigger after
//		if( m_shrinkStartCountdown > 0 )
//			m_shrinkStartCountdown--;
//		if( m_shrinkStartCountdown == 0 )
//		{
//			m_shrinkStarted = TRUE;
//
//			// Timer for how often to change
//			m_changeIntervalCountdown = md->m_changeInterval;
//			// Marker to make sure we end up precisely correct regardless of interval
//			m_shrinkDeadline = TheGameLogic->getFrame() + md->m_shrinkTime;
//			// Figure out how much to change each interval
//			Real visionDiff = me->getShroudClearingRange() - md->m_finalVision;
//			m_visionChangePerInterval = -visionDiff/(md->m_shrinkTime/md->m_changeInterval);
//		}
//		else
//		{
//			return UPDATE_SLEEP_NONE;// waiting to start
//		}
//	}

//	if( m_changeIntervalCountdown > 0 )
//		m_changeIntervalCountdown--;
//	if( m_changeIntervalCountdown != 0 )
//		return UPDATE_SLEEP_NONE;
//	// reset per change timer
//	m_changeIntervalCountdown = getDynamicShroudClearingRangeUpdateModuleData()->m_changeInterval;
//	UnsignedInt currentFrame = TheGameLogic->getFrame();

//	if( currentFrame > m_shrinkDeadline )
//	{
//		// Time to stop doing anything.
//		me->setShroudClearingRange( getDynamicShroudClearingRangeUpdateModuleData()->m_finalVision );
//		m_shrinkFinished = TRUE;
//		killGridDecals();
//	}
//	else
//	{
//		// bump the vision again
//		me->setShroudClearingRange( me->getShroudClearingRange() + m_visionChangePerInterval );
//	}
//	return UPDATE_SLEEP_NONE;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void DynamicShroudClearingRangeUpdate::crc( Xfer *xfer )
{

	// extend base class
	UpdateModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void DynamicShroudClearingRangeUpdate::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpdateModule::xfer( xfer );

	// shrink start countdown
	xfer->xferInt( &m_stateCountDown );

	xfer->xferInt( &m_totalFrames );

	xfer->xferUnsignedInt( &m_growStartDeadline );

	xfer->xferUnsignedInt( &m_sustainDeadline );

	xfer->xferUnsignedInt( &m_shrinkStartDeadline );

	xfer->xferUnsignedInt( &m_doneForeverFrame );

	xfer->xferUnsignedInt( &m_changeIntervalCountdown );

	// vision change per interval
	xfer->xferBool( &m_decalsCreated );

	xfer->xferReal( &m_visionChangePerInterval );

	xfer->xferReal( &m_nativeClearingRange );
	xfer->xferReal( &m_currentClearingRange );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void DynamicShroudClearingRangeUpdate::loadPostProcess()
{

	// extend base class
	UpdateModule::loadPostProcess();

}
