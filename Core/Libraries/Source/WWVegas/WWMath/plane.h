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

/* $Header: /Commando/Code/wwmath/plane.h 16    5/05/01 5:48p Jani_p $ */
/***********************************************************************************************
 ***                            Confidential - Westwood Studios                              ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Voxel Technology                                             *
 *                                                                                             *
 *                    File Name : PLANE.h                                                      *
 *                                                                                             *
 *                   Programmer : Greg Hjelstrom                                               *
 *                                                                                             *
 *                   Start Date : 03/17/97                                                     *
 *                                                                                             *
 *                  Last Update : March 17, 1997 [GH]                                          *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "always.h"
#include "vector3.h"
#include "sphere.h"

/*
** PlaneClass
**
** 3D-planes.  This class uses the Normal+Distance description of a plane.
** The relationship for all points (p) on the plane is given by:
**
**   N.X * p.X + N.Y * p.Y + N.Z * p.Z = D
**
** BEWARE, if you are used to the Ax + By + Cz + D = 0 description, the
** sign of the D value is inverted.
*/

class PlaneClass
{
public:

	enum { FRONT = 0, BACK, ON };

	enum IntersectionResType
	{
		NO_INTERSECTION = 0, // No intersection when the line is parallel to the plane
		INSIDE_SEGMENT = 1, // The segment INSIDE point A and B intersects the plane
		OUTSIDE_LINE = 2, // The infinite line OUTSIDE point A and B intersects the plane
	};

	Vector3	N;			// Normal of the plane
	float		D;			// Distance along the normal from the origin

	PlaneClass() : N(0.0f,0.0f,1.0f), D(0.0f) { }

	/*
	** Plane initialization:
	** a,b,c,d - explicitly set the four coefficients (note the sign of d!)
	** normal,dist - explicitly set the normal and distance
	** normal,point - compute plane with normal, containing point
	** p1,p2,p3 - compute plane containing three points
	*/
	PlaneClass(float nx,float ny,float nz,float dist);
	PlaneClass(const Vector3 & normal,float dist);
	PlaneClass(const Vector3 & normal,const Vector3 & point);
	PlaneClass(const Vector3 & point1,const Vector3 & point2,const Vector3 & point3);

	inline void Set(float a,float b,float c,float d);
	inline void Set(const Vector3 & normal,float dist);
	inline void Set(const Vector3 & normal,const Vector3 & point);
	inline void Set(const Vector3 & point1,const Vector3 & point2,const Vector3 & point3);

	PlaneClass::IntersectionResType Compute_Intersection(const Vector3 & p0,const Vector3 & p1,float * set_t) const;
	bool In_Front(const Vector3 & point) const;
	bool In_Front(const SphereClass & sphere) const;
	bool In_Front_Or_Intersecting(const SphereClass & sphere) const;

	static void Intersect_Planes(const PlaneClass & a, const PlaneClass & b, Vector3 *line_dir, Vector3 *line_point);
};

inline PlaneClass::PlaneClass(float nx,float ny,float nz,float dist)
{
	Set(nx,ny,nz,dist);
}

inline PlaneClass::PlaneClass(const Vector3 & normal,float dist)
{
	Set(normal,dist);
}

inline PlaneClass::PlaneClass(const Vector3 & normal,const Vector3 & point)
{
	Set(normal,point);
}

inline PlaneClass::PlaneClass(const Vector3 & point1, const Vector3 & point2, const Vector3 & point3)
{
	Set(point1,point2,point3);
}

inline void PlaneClass::Set(float a,float b,float c,float d)
{
	N.X = a;
	N.Y = b;
	N.Z = c;
	D = d;
}

inline void PlaneClass::Set(const Vector3 & normal,float dist)
{
	N = normal;
	D = dist;
}

inline void PlaneClass::Set(const Vector3 & normal,const Vector3 & point)
{
	N = normal;
	D = Vector3::Dot_Product(normal , point);
}


inline void PlaneClass::Set(const Vector3 & point1, const Vector3 & point2, const Vector3 & point3)
{
#ifdef ALLOW_TEMPORARIES
	N = Vector3::Cross_Product((point2 - point1), (point3 - point1));
#else
	Vector3::Cross_Product((point2 - point1), (point3 - point1), &N);
#endif
	if (N != Vector3(0.0f, 0.0f, 0.0f)) {
		// Points are not colinear. Normalize N and calculate D.
		N.Normalize();
		D = Vector3::Dot_Product(N, point1);
	} else {
		// They are colinear - return default plane (constructors can't fail).
		N = Vector3(0.0f, 0.0f, 1.0f);
		D = 0.0f;
	}
}

inline PlaneClass::IntersectionResType PlaneClass::Compute_Intersection(const Vector3 & p0,const Vector3 & p1,float * set_t) const
{
	float num,den;
	den = Vector3::Dot_Product(N,p1-p0);

	/*
	** If the denominator is zero, the ray is parallel to the plane
	*/
	if (den == 0.0f) {
		return PlaneClass::NO_INTERSECTION;
	}

	num = -(Vector3::Dot_Product(N,p0) - D);

	*set_t = num/den;

	/*
	** If t is not between 0 and 1, the line containing the segment intersects
	** the plane but the segment does not
	*/
	if ((*set_t < 0.0f) || (*set_t > 1.0f)) {
		return PlaneClass::OUTSIDE_LINE;
	}

	return PlaneClass::INSIDE_SEGMENT;
}

inline bool PlaneClass::In_Front(const Vector3 & point) const
{
	float dist = Vector3::Dot_Product(point,N);
	return (dist > D);
}

// This function returns true if the sphere is in front of the plane.
inline bool PlaneClass::In_Front(const SphereClass & sphere) const
{
	float dist = Vector3::Dot_Product(sphere.Center,N);
	return ((dist - D) >= sphere.Radius);
}

// This function will return 1 if any part of the sphere is in front of the plane.
// (i.e. if the sphere is entirely in front of the plane or if it intersects the plane).
inline bool PlaneClass::In_Front_Or_Intersecting(const SphereClass & sphere) const
{
	float dist = Vector3::Dot_Product(sphere.Center , N);
	return ((D - dist) < sphere.Radius);
}

inline void PlaneClass::Intersect_Planes(const PlaneClass & a, const PlaneClass & b, Vector3 *line_dir, Vector3 *line_point)
{
	// Method used is from "plane-to-plane intersection", Graphics Gems III, pp. 233-235.

	// Find line of intersection. First find direction vector of line:
	Vector3::Cross_Product(a.N, b.N, line_dir);

	// Now find point on line. How we do it depends on what the largest coordinate of the
	// direction vector is.
	Vector3 abs_dir = *line_dir;
	abs_dir.Update_Max(-abs_dir);
	if (abs_dir.X > abs_dir.Y) {
		if (abs_dir.X > abs_dir.Z) {
			// X largest
			float ool = 1.0f / line_dir->X;
			line_point->Y = (b.N.Z * a.D - a.N.Z * b.D) * ool;
			line_point->Z = (a.N.Y * b.D - b.N.Y * a.D) * ool;
			line_point->X = 0.0f;
		} else {
			// Z largest
			float ool = 1.0f / line_dir->Z;
			line_point->X = (b.N.Y * a.D - a.N.Y * b.D) * ool;
			line_point->Y = (a.N.X * b.D - b.N.X * a.D) * ool;
			line_point->Z = 0.0f;
		}
	} else {
		if (abs_dir.Y > abs_dir.Z) {
			// Y largest
			float ool = 1.0f / line_dir->Y;
			line_point->Z = (b.N.X * a.D - a.N.X * b.D) * ool;
			line_point->X = (a.N.Z * b.D - b.N.Z * a.D) * ool;
			line_point->Y = 0.0f;
		} else {
			// Z largest
			float ool = 1.0f / line_dir->Z;
			line_point->X = (b.N.Y * a.D - a.N.Y * b.D) * ool;
			line_point->Y = (a.N.X * b.D - b.N.X * a.D) * ool;
			line_point->Z = 0.0f;
		}
	}

	// Normalize direction vector (we do it here because we needed the non-normalized version to
	// find the point).
	line_dir->Normalize();
}
