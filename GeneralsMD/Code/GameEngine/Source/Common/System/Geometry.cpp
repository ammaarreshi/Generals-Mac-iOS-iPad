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

// FILE: Geometry.cpp /////////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, Aug 2002
// Desc:
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#define DEFINE_GEOMETRY_NAMES

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "Common/Geometry.h"
#include "Common/INI.h"
#include "Common/RandomValue.h"
#include "Common/Xfer.h"


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//=============================================================================
/*static*/ void GeometryInfo::parseGeometryType( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	((GeometryInfo*)store)->m_type = (GeometryType)INI::scanIndexList(ini->getNextToken(), GeometryNames);
	((GeometryInfo*)store)->calcBoundingStuff();
}

//=============================================================================
/*static*/ void GeometryInfo::parseGeometryIsSmall( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	((GeometryInfo*)store)->m_isSmall = INI::scanBool(ini->getNextToken());
	((GeometryInfo*)store)->calcBoundingStuff();
}

//=============================================================================
/*static*/ void GeometryInfo::parseGeometryHeight( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	((GeometryInfo*)store)->m_height = INI::scanReal(ini->getNextToken());
	((GeometryInfo*)store)->calcBoundingStuff();
}

//=============================================================================
/*static*/ void GeometryInfo::parseGeometryMajorRadius( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	((GeometryInfo*)store)->m_majorRadius = INI::scanReal(ini->getNextToken());
	((GeometryInfo*)store)->calcBoundingStuff();
}

//=============================================================================
/*static*/ void GeometryInfo::parseGeometryMinorRadius( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	((GeometryInfo*)store)->m_minorRadius = INI::scanReal(ini->getNextToken());
	((GeometryInfo*)store)->calcBoundingStuff();
}

//-----------------------------------------------------------------------------
void GeometryInfo::set(GeometryType type, Bool isSmall, Real height, Real majorRadius, Real minorRadius)
{
	m_type = type;
	m_isSmall = isSmall;

	switch(m_type)
	{
		case GEOMETRY_SPHERE:
			m_majorRadius = majorRadius;
			m_minorRadius = majorRadius;
			m_height = majorRadius;
			break;

		case GEOMETRY_CYLINDER:
			m_majorRadius = majorRadius;
			m_minorRadius = majorRadius;
			m_height = height;
			break;

		case GEOMETRY_BOX:
			m_majorRadius = majorRadius;
			m_minorRadius = minorRadius;
			m_height = height;
			break;
	};

	calcBoundingStuff();
}

//-----------------------------------------------------------------------------
static Real calcDotProduct(const Coord3D& a, const Coord3D& b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

//-----------------------------------------------------------------------------
static Real calcDistSquared(const Coord3D& a, const Coord3D& b)
{
	return sqr(a.x - b.x) + sqr(a.y - b.y) + sqr(a.z - b.z);
}

//-----------------------------------------------------------------------------
static Real calcPointToLineDistSquared(const Coord3D& pt, const Coord3D& lineStart, const Coord3D& lineEnd)
{
	Coord3D line, lineToPt, closest;

	line.x = lineEnd.x - lineStart.x;
	line.y = lineEnd.y - lineStart.y;
	line.z = lineEnd.z - lineStart.z;

	lineToPt.x = pt.x - lineStart.x;
	lineToPt.y = pt.y - lineStart.y;
	lineToPt.z = pt.z - lineStart.z;

	Real dot = calcDotProduct(lineToPt, line);
	if (dot <= 0.0f)
	{
		// angle is obtuse
		return calcDistSquared(pt, lineStart);
	}

	Real lineLenSqr = calcDistSquared(lineStart, lineEnd);
	DEBUG_ASSERTCRASH(lineLenSqr==calcDotProduct(line,line),("hmm"));
	if (lineLenSqr <= dot)
	{
		return calcDistSquared(pt, lineEnd);
	}

  Real tmp = dot / lineLenSqr;

	closest.x = lineStart.x + tmp * line.x;
	closest.y = lineStart.y + tmp * line.y;
	closest.z = lineStart.z + tmp * line.z;

  return calcDistSquared(pt, closest);
}

//=============================================================================
Bool GeometryInfo::isIntersectedByLineSegment(const Coord3D& loc, const Coord3D& from, const Coord3D& to) const
{
	DEBUG_CRASH(("this call does not work properly for nonspheres yet. use with caution."));

	/// @todo srj -- treats everything as a sphere for now. fix.
	Real distSquared = calcPointToLineDistSquared(loc, from, to);
	return distSquared <= sqr(getBoundingSphereRadius());
}

//=============================================================================
// given an object with this geom, located at 'pos', and another obj with the given
// pos & geom, calc the min and max pitches from this to that.
void GeometryInfo::calcPitches(const Coord3D& thisPos, const GeometryInfo& that, const Coord3D& thatPos,
	Real& minPitch, Real& maxPitch) const
{
	Coord3D thisCenter;
	getCenterPosition(thisPos, thisCenter);

	Real dxy = WWMath::SqrtOrigin(sqr(thatPos.x - thisCenter.x) + sqr(thatPos.y - thisCenter.y));

	Real dz;

	/** @todo srj -- this could be better, by calcing it for all the corners, not just top-center
		and bottom-center... oh well */
	dz = (thatPos.z + that.getMaxHeightAbovePosition()) - thisCenter.z;
	maxPitch = WWMath::Atan2Origin(dz, dxy);

	dz = (thatPos.z - that.getMaxHeightBelowPosition()) - thisCenter.z;
	minPitch = WWMath::Atan2Origin(dz, dxy);
}

//=============================================================================
// given an object with this geom, SET how far above the object's canonical position its max z should extend.
void GeometryInfo::setMaxHeightAbovePosition(Real z)
{
	switch(m_type)
	{
		case GEOMETRY_SPHERE:
			m_majorRadius = z;
			break;

		case GEOMETRY_BOX:
		case GEOMETRY_CYLINDER:
			m_height = z;
			break;
	};

	calcBoundingStuff();
}

//=============================================================================
// given an object with this geom, how far above the object's canonical position does its max z extend?
Real GeometryInfo::getMaxHeightAbovePosition() const
{
	switch(m_type)
	{
		case GEOMETRY_SPHERE:
			return m_majorRadius;

		case GEOMETRY_BOX:
		case GEOMETRY_CYLINDER:
			return m_height;
	};

	return 0.0f;
}

//=============================================================================
// given an object with this geom, how far below the object's canonical position does its max z extend?
Real GeometryInfo::getMaxHeightBelowPosition() const
{
	switch(m_type)
	{
		case GEOMETRY_SPHERE:
			return m_majorRadius;

		case GEOMETRY_BOX:
		case GEOMETRY_CYLINDER:
			return 0.0f;
	};

	return 0.0f;
}

//=============================================================================
// given an object with this geom, located at 'pos', where is the "center" of the geometry?
Real GeometryInfo::getZDeltaToCenterPosition() const
{
	return (m_type == GEOMETRY_SPHERE) ? 0.0f : (m_height * 0.5f);
}

//=============================================================================
// given an object with this geom, located at 'pos', where is the "center" of the geometry?
void GeometryInfo::getCenterPosition(const Coord3D& pos, Coord3D& center) const
{
	center = pos;
	center.z += getZDeltaToCenterPosition();
}

//=============================================================================
void GeometryInfo::expandFootprint(Real radius)
{
	m_majorRadius += radius;
	m_minorRadius += radius;
	calcBoundingStuff();
}

//=============================================================================
void GeometryInfo::get2DBounds(const Coord3D& geomCenter, Real angle, Region2D& bounds) const
{
	switch(m_type)
	{
		case GEOMETRY_SPHERE:
		case GEOMETRY_CYLINDER:
		{
			bounds.lo.x = geomCenter.x - m_majorRadius;
			bounds.lo.y = geomCenter.y - m_majorRadius;
			bounds.hi.x = geomCenter.x + m_majorRadius;
			bounds.hi.y = geomCenter.y + m_majorRadius;
			break;
		}

		case GEOMETRY_BOX:
		{
			Real c = (Real)WWMath::CosTrig(angle);
			Real s = (Real)WWMath::SinTrig(angle);
			Real exc = m_majorRadius*c;
			Real eyc = m_minorRadius*c;
			Real exs = m_majorRadius*s;
			Real eys = m_minorRadius*s;
			Real x,y;
			x = geomCenter.x - exc - eys;
			y = geomCenter.y + eyc - exs;
			bounds.lo.x = x;
			bounds.lo.y = y;
			bounds.hi.x = x;
			bounds.hi.y = y;

			x = geomCenter.x + exc - eys;
			y = geomCenter.y + eyc + exs;
			if (bounds.lo.x > x) bounds.lo.x = x;
			if (bounds.lo.y > y) bounds.lo.y = y;
			if (bounds.hi.x < x) bounds.hi.x = x;
			if (bounds.hi.y < y) bounds.hi.y = y;

			x = geomCenter.x + exc + eys;
			y = geomCenter.y - eyc + exs;
			if (bounds.lo.x > x) bounds.lo.x = x;
			if (bounds.lo.y > y) bounds.lo.y = y;
			if (bounds.hi.x < x) bounds.hi.x = x;
			if (bounds.hi.y < y) bounds.hi.y = y;

			x = geomCenter.x - exc + eys;
			y = geomCenter.y - eyc - exs;
 			if (bounds.lo.x > x) bounds.lo.x = x;
			if (bounds.lo.y > y) bounds.lo.y = y;
			if (bounds.hi.x < x) bounds.hi.x = x;
			if (bounds.hi.y < y) bounds.hi.y = y;

			break;
		}
	}
}

//=============================================================================
void GeometryInfo::clipPointToFootprint(const Coord3D& geomCenter, Coord3D& ptToClip) const
{
	switch(m_type)
	{
		case GEOMETRY_SPHERE:
		case GEOMETRY_CYLINDER:
		{
			Real dx = ptToClip.x - geomCenter.x;
			Real dy = ptToClip.y - geomCenter.y;
			Real radius = WWMath::SqrtOrigin(sqr(dx) + sqr(dy));
			if (radius > m_majorRadius)
			{
				Real ratio = m_majorRadius / radius;
				ptToClip.x = geomCenter.x + dx * ratio;
				ptToClip.y = geomCenter.y + dy * ratio;
			}
			break;
		}

		case GEOMETRY_BOX:
		{
			ptToClip.x = clamp(geomCenter.x - m_majorRadius, ptToClip.x, geomCenter.x + m_majorRadius);
			ptToClip.y = clamp(geomCenter.y - m_minorRadius, ptToClip.y, geomCenter.y + m_minorRadius);
			break;
		}
	};
}

//=============================================================================
inline Bool isWithin(Real a, Real b, Real c) { return a<=b && b<=c; }

//=============================================================================
Bool GeometryInfo::isPointInFootprint(const Coord3D& geomCenter, const Coord3D& pt) const
{
	switch(m_type)
	{
		case GEOMETRY_SPHERE:
		case GEOMETRY_CYLINDER:
		{
			Real dx = pt.x - geomCenter.x;
			Real dy = pt.y - geomCenter.y;
			Real radius = WWMath::SqrtOrigin(sqr(dx) + sqr(dy));
			return (radius <= m_majorRadius);
			break;
		}

		case GEOMETRY_BOX:
		{
			return isWithin(geomCenter.x - m_majorRadius, pt.x, geomCenter.x + m_majorRadius) &&
							isWithin(geomCenter.y - m_minorRadius, pt.y, geomCenter.y + m_minorRadius);
		}
	};
	return false;
}

//=============================================================================
void GeometryInfo::makeRandomOffsetWithinFootprint(Coord3D& pt) const
{
	switch(m_type)
	{
		case GEOMETRY_SPHERE:
		case GEOMETRY_CYLINDER:
		{
#if 1
			// this is a better technique than the more obvious radius-and-angle
			// one, below, because the latter tends to clump more towards the center.
			Real maxDistSqr = sqr(m_majorRadius);
			Real distSqr;
			do
			{
				pt.x = GameLogicRandomValueReal(-m_majorRadius, m_majorRadius);
				pt.y = GameLogicRandomValueReal(-m_majorRadius, m_majorRadius);
				pt.z = 0.0f;
				distSqr = sqr(pt.x) + sqr(pt.y);
			} while (distSqr > maxDistSqr);
#else
			Real radius = GameLogicRandomValueReal(0.0f, m_boundingCircleRadius);
			Real angle = GameLogicRandomValueReal(-PI, PI);
			pt.x = radius * Cos(angle);
			pt.y = radius * Sin(angle);
			pt.z = 0.0f;
#endif
			break;
		}

		case GEOMETRY_BOX:
		{
			pt.x = GameLogicRandomValueReal(-m_majorRadius, m_majorRadius);
			pt.y = GameLogicRandomValueReal(-m_minorRadius, m_minorRadius);
			pt.z = 0.0f;
			break;
		}
	};
}

//=============================================================================
void GeometryInfo::makeRandomOffsetOnPerimeter(Coord3D& pt) const
{
	switch(m_type)
	{
		case GEOMETRY_SPHERE:
		case GEOMETRY_CYLINDER:
		{
			DEBUG_CRASH( ("GeometryInfo::makeRandomOffsetOnPerimeter() not implemented for SPHERE or CYLINDER extents. Using position.") );

			//Kris: Did not have time nor need to support non-box extents. I added this feature for script placement
			//      of boobytraps.
			pt.x = 0.0f;
			pt.y = 0.0f;
			break;
		}

		case GEOMETRY_BOX:
		{
			if( GameLogicRandomValueReal( 0.0f, 1.0f ) < 0.5f )
			{
				//Pick random point on x axis.
				pt.x = GameLogicRandomValueReal(-m_majorRadius, m_majorRadius);

				//Min or max the y axis value
				if( GameLogicRandomValueReal( 0.0f, 1.0f ) < 0.5f )
					pt.y = -m_minorRadius;
				else
					pt.y = m_minorRadius;
			}
			else
			{
				//Pick random point on y axis.
				pt.y = GameLogicRandomValueReal(-m_minorRadius, m_minorRadius);

				//Min or max the x axis value
				if( GameLogicRandomValueReal( 0.0f, 1.0f ) < 0.5f )
					pt.x = -m_majorRadius;
				else
					pt.x = m_majorRadius;
			}
			pt.z = 0.0f;
			break;
		}
	};
}

//=============================================================================
Real GeometryInfo::getFootprintArea() const
{
	switch(m_type)
	{
		case GEOMETRY_SPHERE:
		case GEOMETRY_CYLINDER:
		{
			return PI * sqr(m_boundingCircleRadius);
		}

		case GEOMETRY_BOX:
		{
			return 4.0f * m_majorRadius * m_minorRadius;
		}
	};

	DEBUG_CRASH(("should never get here"));
	return 0.0f;
}

//=============================================================================
void GeometryInfo::calcBoundingStuff()
{
	switch(m_type)
	{
		case GEOMETRY_SPHERE:
		{
			m_boundingSphereRadius = m_majorRadius;
			m_boundingCircleRadius = m_majorRadius;
			break;
		}
		case GEOMETRY_CYLINDER:
		{
			m_boundingCircleRadius = m_majorRadius;

			m_boundingSphereRadius = m_height*0.5;
			if (m_boundingSphereRadius < m_majorRadius)
				m_boundingSphereRadius = m_majorRadius;
			break;
		}

		case GEOMETRY_BOX:
		{
			m_boundingCircleRadius = WWMath::SqrtOrigin(sqr(m_majorRadius) + sqr(m_minorRadius));
			m_boundingSphereRadius = WWMath::SqrtOrigin(sqr(m_majorRadius) + sqr(m_minorRadius) + sqr(m_height*0.5));
			break;
		}
	};
}

#if defined(RTS_DEBUG)
//=============================================================================
void GeometryInfo::tweakExtents(ExtentModType extentModType, Real extentModAmount)
{
	switch(extentModType)
	{
		case EXTENTMOD_HEIGHT:
			m_height += extentModAmount;
			break;
		case EXTENTMOD_MAJOR:
			m_majorRadius += extentModAmount;
			break;
		case EXTENTMOD_MINOR:
			m_minorRadius += extentModAmount;
			break;
		case EXTENTMOD_TYPE:
			m_type = (GeometryType)((m_type + ((extentModType == EXTENTMOD_TYPE)?1:0)) % GEOMETRY_NUM_TYPES);
			break;
	}
	m_isSmall = false;
	calcBoundingStuff();
}
#endif

#if defined(RTS_DEBUG)
//=============================================================================
AsciiString GeometryInfo::getDescriptiveString() const
{
	AsciiString msg;
	msg.format("%d/%d(%g %g %g)", (Int)m_type, (Int)m_isSmall, m_majorRadius, m_minorRadius, m_height);
	return msg;
}
#endif

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void GeometryInfo::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void GeometryInfo::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// type
	xfer->xferUser( &m_type, sizeof( GeometryType ) );

	// is small
	xfer->xferBool( &m_isSmall );

	// height
	xfer->xferReal( &m_height );

	// major radius
	xfer->xferReal( &m_majorRadius );

	// minor radius
	xfer->xferReal( &m_minorRadius );

	// bouncing circle radius
	xfer->xferReal( &m_boundingCircleRadius );

	// bounding sphere radius
	xfer->xferReal( &m_boundingSphereRadius );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void GeometryInfo::loadPostProcess()
{

}
