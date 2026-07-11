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


#include "PreRTS.h"
#include "Common/BezierSegment.h"
#include "Common/BezFwdIterator.h"

#ifdef _WIN32
#include <d3dx8math.h>
#elif defined(SAGE_USE_GLM)
#include <glm/glm.hpp>
#else
#error "Missing a math library"
#endif

//-------------------------------------------------------------------------------------------------
BezierSegment::BezierSegment()
{
	for(int i=0; i < 4; i++)
		m_controlPoints[i].zero();
}

//-------------------------------------------------------------------------------------------------
BezierSegment::BezierSegment(Real x0, Real y0, Real z0,
														 Real x1, Real y1, Real z1,
														 Real x2, Real y2, Real z2,
														 Real x3, Real y3, Real z3)
{
	m_controlPoints[0].x = x0;
	m_controlPoints[0].y = y0;
	m_controlPoints[0].z = z0;

	m_controlPoints[1].x = x1;
	m_controlPoints[1].y = y1;
	m_controlPoints[1].z = z1;

	m_controlPoints[2].x = x2;
	m_controlPoints[2].y = y2;
	m_controlPoints[2].z = z2;

	m_controlPoints[3].x = x3;
	m_controlPoints[3].y = y3;
	m_controlPoints[3].z = z3;
}

BezierSegment::BezierSegment(Real cp[12])
{
	m_controlPoints[0].x = cp[0];
	m_controlPoints[0].y = cp[1];
	m_controlPoints[0].z = cp[2];

	m_controlPoints[1].x = cp[3];
	m_controlPoints[1].y = cp[4];
	m_controlPoints[1].z = cp[5];

	m_controlPoints[2].x = cp[6];
	m_controlPoints[2].y = cp[7];
	m_controlPoints[2].z = cp[8];

	m_controlPoints[3].x = cp[9];
	m_controlPoints[3].y = cp[10];
	m_controlPoints[3].z = cp[11];
}

BezierSegment::BezierSegment(const Coord3D& cp0, const Coord3D& cp1, const Coord3D& cp2, const Coord3D& cp3)
{
	m_controlPoints[0] = cp0;
	m_controlPoints[1] = cp1;
	m_controlPoints[2] = cp2;
	m_controlPoints[3] = cp3;
}

BezierSegment::BezierSegment(Coord3D cp[4])
{
	m_controlPoints[0] = cp[0];
	m_controlPoints[1] = cp[1];
	m_controlPoints[2] = cp[2];
	m_controlPoints[3] = cp[3];
}


//-------------------------------------------------------------------------------------------------
void BezierSegment::evaluateBezSegmentAtT(Real tValue, Coord3D *outResult) const

{
	if (!outResult)
		return;

#ifndef SAGE_USE_GLM
	D3DXVECTOR4	tVec(tValue * tValue * tValue, tValue * tValue, tValue, 1);

	D3DXVECTOR4 xCoords(m_controlPoints[0].x, m_controlPoints[1].x, m_controlPoints[2].x, m_controlPoints[3].x);
	D3DXVECTOR4 yCoords(m_controlPoints[0].y, m_controlPoints[1].y, m_controlPoints[2].y, m_controlPoints[3].y);
	D3DXVECTOR4 zCoords(m_controlPoints[0].z, m_controlPoints[1].z, m_controlPoints[2].z, m_controlPoints[3].z);

	D3DXVECTOR4 tResult;
	D3DXVec4Transform(&tResult, &tVec, &BezierSegment::s_bezBasisMatrix);

	outResult->x = D3DXVec4Dot(&xCoords, &tResult);
	outResult->y = D3DXVec4Dot(&yCoords, &tResult);
	outResult->z = D3DXVec4Dot(&zCoords, &tResult);
#else // SAGE_USE_GLM
	glm::vec4 tVec(tValue * tValue * tValue, tValue * tValue, tValue, 1);

	glm::vec4 xCoords(m_controlPoints[0].x, m_controlPoints[1].x, m_controlPoints[2].x, m_controlPoints[3].x);
	glm::vec4 yCoords(m_controlPoints[0].y, m_controlPoints[1].y, m_controlPoints[2].y, m_controlPoints[3].y);
	glm::vec4 zCoords(m_controlPoints[0].z, m_controlPoints[1].z, m_controlPoints[2].z, m_controlPoints[3].z);

	glm::vec4 tResult = BezierSegment::s_bezBasisMatrix * tVec;

	outResult->x = glm::dot(xCoords, tResult);
	outResult->y = glm::dot(yCoords, tResult);
	outResult->z = glm::dot(zCoords, tResult);
#endif
}

//-------------------------------------------------------------------------------------------------
void BezierSegment::getSegmentPoints(Int numSegments, VecCoord3D *outResult) const
{
	if (!outResult) {
		return;
	}

	outResult->clear();
	outResult->resize(numSegments);

	BezFwdIterator iter(numSegments, this);
	iter.start();
	Int i = 0;
	while (!iter.done()) {
		(*outResult)[i] = iter.getCurrent();
		++i;
		iter.next();
	}
}

//-------------------------------------------------------------------------------------------------
// This function isn't terribly fast. There are alternatives, and if this is too slow, we can
// take a look at the other approximations.
// There is no known close-form solution to this problem.
Real BezierSegment::getApproximateLength(Real withinTolerance) const
{
	/*
		How this works:
		We can determine the approximate length of a bezier segment by
		L0 = |(P0,P1)| + |(P1,P2)| + |(P2,P3)|
		L1 = |(P0,P3)|

		The length of the segment is approximately 1/2 L0 + 1/2 L1

		P1__P2
		/		 \
	 P0----P3

		The error in this is L1 - L0. If the error is too much, then we subdivide the curve and
		try again.
	*/

	Coord3D p0p1 = { m_controlPoints[1].x - m_controlPoints[0].x,
									 m_controlPoints[1].y - m_controlPoints[0].y,
									 m_controlPoints[1].z - m_controlPoints[0].z, };

	Coord3D p1p2 = { m_controlPoints[2].x - m_controlPoints[1].x,
									 m_controlPoints[2].y - m_controlPoints[1].y,
									 m_controlPoints[2].z - m_controlPoints[1].z, };

	Coord3D p2p3 = { m_controlPoints[3].x - m_controlPoints[2].x,
									 m_controlPoints[3].y - m_controlPoints[2].y,
									 m_controlPoints[3].z - m_controlPoints[2].z, };

	Coord3D p0p3 = { m_controlPoints[3].x - m_controlPoints[0].x,
									 m_controlPoints[3].y - m_controlPoints[0].y,
									 m_controlPoints[3].z - m_controlPoints[0].z, };

	Real length0 = p0p3.length();
	Real length1 = p0p1.length() + p1p2.length() + p2p3.length();

	if ((length1 - length0) > withinTolerance) {
		BezierSegment seg1, seg2;
		splitSegmentAtT(0.5f, seg1, seg2);
		return (seg1.getApproximateLength(withinTolerance) + seg2.getApproximateLength(withinTolerance));
	}

	return (INT_TO_REAL(length0 + length1) / 2.0f);
}

//-------------------------------------------------------------------------------------------------
void BezierSegment::splitSegmentAtT(Real tValue, BezierSegment &outSeg1, BezierSegment &outSeg2) const
{
	// I think there are faster ways to do this. Could someone clue me in?

	Coord3D p0p1 = { m_controlPoints[1].x - m_controlPoints[0].x,
									 m_controlPoints[1].y - m_controlPoints[0].y,
									 m_controlPoints[1].z - m_controlPoints[0].z, };

	Coord3D p1p2 = { m_controlPoints[2].x - m_controlPoints[1].x,
									 m_controlPoints[2].y - m_controlPoints[1].y,
									 m_controlPoints[2].z - m_controlPoints[1].z, };

	Coord3D p2p3 = { m_controlPoints[3].x - m_controlPoints[2].x,
									 m_controlPoints[3].y - m_controlPoints[2].y,
									 m_controlPoints[3].z - m_controlPoints[2].z, };

	p0p1.scale(tValue);
	p1p2.scale(tValue);
	p2p3.scale(tValue);

	p0p1.add(m_controlPoints[0]);
	p1p2.add(m_controlPoints[1]);
	p2p3.add(m_controlPoints[2]);

	Coord3D triLeft = { p1p2.x - p0p1.x,
											p1p2.y - p0p1.y,
											p1p2.z - p0p1.z, };

	Coord3D triRight = { p2p3.x - p1p2.x,
											 p2p3.y - p1p2.y,
											 p2p3.z - p1p2.z, };

	triLeft.scale(tValue);
	triRight.scale(tValue);

	triLeft.add(p0p1);
	triRight.add(p1p2);

	outSeg1.m_controlPoints[0] = m_controlPoints[0];
	outSeg1.m_controlPoints[1] = p0p1;
	outSeg1.m_controlPoints[2] = triLeft;
	evaluateBezSegmentAtT(tValue, &outSeg1.m_controlPoints[3]);

	outSeg2.m_controlPoints[0] = outSeg1.m_controlPoints[3];
	outSeg2.m_controlPoints[1] = triRight;
	outSeg2.m_controlPoints[2] = p2p3;
	outSeg2.m_controlPoints[3] = m_controlPoints[3];
}

//-------------------------------------------------------------------------------------------------
// The Basis Matrix for a bezier segment
#ifndef SAGE_USE_GLM
const D3DXMATRIX BezierSegment::s_bezBasisMatrix(
	-1.0f,  3.0f, -3.0f,  1.0f,
	 3.0f, -6.0f,  3.0f,  0.0f,
	-3.0f,  3.0f,  0.0f,  0.0f,
	 1.0f,  0.0f,  0.0f,  0.0f
);
#else
const glm::mat4 BezierSegment::s_bezBasisMatrix(
	-1.0f,  3.0f, -3.0f,  1.0f,
	 3.0f, -6.0f,  3.0f,  0.0f,
	-3.0f,  3.0f,  0.0f,  0.0f,
	 1.0f,  0.0f,  0.0f,  0.0f
);
#endif
