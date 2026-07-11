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

// FILE: BaseType.h ///////////////////////////////////////////////////////////
//
// Project:  RTS3
//
// Basic types and constants
// Author: Michael S. Booth, January 1995, September 2000
//
///////////////////////////////////////////////////////////////////////////////

// tell the compiler to only load this file once

#pragma once

#include "Lib/BaseTypeCore.h"
#include "Lib/trig.h"

//-----------------------------------------------------------------------------
typedef wchar_t WideChar;  ///< multi-byte character representations

//-----------------------------------------------------------------------------
template <typename NUM>
inline NUM sqr(NUM x)
{
	return x*x;
}

template <typename NUM>
inline NUM clamp(NUM lo, NUM val, NUM hi)
{
	if (val < lo) return lo;
	else if (val > hi) return hi;
	else return val;
}

template <typename NUM>
inline int sign(NUM x)
{
	if (x > 0) return 1;
	else if (x < 0) return -1;
	else return 0;
}

template <typename NUM>
inline NUM highestBit(NUM x)
{
	static_assert(sizeof(NUM) <= 8, "NUM must be 8 bytes or less");
	UnsignedInt64 y = static_cast<UnsignedInt64>(x);

	y |= (y >> 1);
	y |= (y >> 2);
	y |= (y >> 4);
	y |= (y >> 8);
	y |= (y >> 16);
	y |= (y >> 32);

	return static_cast<NUM>(y & ~(y >> 1));
}

template <typename PTR>
inline PTR maxPtr(PTR x, PTR y) noexcept
{
	static_assert(std::is_pointer<PTR>::value, "maxPtr is for pointer types only!");

	if (x == nullptr)
		return y;

	if (y == nullptr)
		return x;

	if (x > y)
		return x;

	return y;
}

template <typename PTR>
inline PTR minPtr(PTR x, PTR y) noexcept
{
	static_assert(std::is_pointer<PTR>::value, "minPtr is for pointer types only!");

	if (x == nullptr)
		return y;

	if (y == nullptr)
		return x;

	if (x < y)
		return x;

	return y;
}

// TheSuperHackers @refactor JohnsterID 24/01/2026 Add lowercase min/max templates for GameEngine layer.
// GameEngine code typically uses BaseType.h, but may include WWVegas headers (which define min/max in always.h).
// Header guard prevents duplicate definitions. VC6's <algorithm> lacks std::min/std::max.
#ifndef _MIN_MAX_TEMPLATES_DEFINED_
#define _MIN_MAX_TEMPLATES_DEFINED_

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

template <typename T>
inline T min(T a, T b) { return (a < b) ? a : b; }

template <typename T>
inline T max(T a, T b) { return (a > b) ? a : b; }

#endif // _MIN_MAX_TEMPLATES_DEFINED_

//-----------------------------------------------------------------------------
inline Real rad2deg(Real rad) { return rad * (180/PI); }
inline Real deg2rad(Real rad) { return rad * (PI/180); }

//-----------------------------------------------------------------------------
// For twiddling bits
//-----------------------------------------------------------------------------
// TheSuperHackers @build xezon 17/03/2025 Renames BitTest to BitIsSet to prevent conflict with BitTest macro from winnt.h
#define BitIsSet( x, i ) ( ( (x) & (i) ) != 0 )
#define BitSet( x, i ) ( (x) |= (i) )
#define BitClear( x, i ) ( (x ) &= ~(i) )
#define BitToggle( x, i ) ( (x) ^= (i) )

//-------------------------------------------------------------------------------------------------

// note, this function depends on the cpu rounding mode, which we set to CHOP every frame,
// but apparently tends to be left in unpredictable modes by various system bits of
// code, so use this function with caution -- it might not round in the way you want.
__forceinline long fast_float2long_round(float f)
{
	long i;

#if defined(_MSC_VER) && _MSC_VER < 1300
	__asm {
		fld [f]
		fistp [i]
	}
#else
	i = lroundf(f);
#endif

	return i;
}

// super fast float trunc routine, works always (independent of any FPU modes)
// code courtesy of Martin Hoffesommer (grin)
__forceinline float fast_float_trunc(float f)
{
#if defined(_MSC_VER) && _MSC_VER < 1300
  _asm
  {
    mov ecx,[f]
    shr ecx,23
    mov eax,0xff800000
    xor ebx,ebx
    sub cl,127
    cmovc eax,ebx
    sar eax,cl
    and [f],eax
  }
  return f;
#else
  unsigned x = *(unsigned *)&f;
  unsigned char exp = x >> 23;
  int mask = exp < 127 ? 0 : 0xff800000;
  exp -= 127;
  mask >>= exp & 31;
  x &= mask;
  return *(float *)&x;
#endif
}

// same here, fast floor function
__forceinline float fast_float_floor(float f)
{
  static unsigned almost1=(126<<23)|0x7fffff;
  if (*(unsigned *)&f &0x80000000)
    f-=*(float *)&almost1;
  return fast_float_trunc(f);
}

// same here, fast ceil function
__forceinline float fast_float_ceil(float f)
{
  static unsigned almost1=(126<<23)|0x7fffff;
  if ( (*(unsigned *)&f &0x80000000)==0)
    f+=*(float *)&almost1;
  return fast_float_trunc(f);
}

//-------------------------------------------------------------------------------------------------
#define REAL_TO_INT(x)						((Int)(x))
#define REAL_TO_UNSIGNEDINT(x)		((UnsignedInt)(x))
#define REAL_TO_SHORT(x)					((Short)(x))
#define REAL_TO_UNSIGNEDSHORT(x)	((UnsignedShort)(x))
#define REAL_TO_BYTE(x)						((Byte)(x))
#define REAL_TO_UNSIGNEDBYTE(x)		((UnsignedByte)(x))
#define REAL_TO_CHAR(x)						((Char)(x))
#define DOUBLE_TO_REAL(x)					((Real)(x))
#define DOUBLE_TO_INT(x)					((Int)(x))
#define INT_TO_REAL(x)						((Real)(x))

// once we've ceiled/floored, trunc and round are identical, and currently, round is faster... (srj)
// GeneralsX @bugfix BenderAI 10/07/2026 Preserve the retail Generals ceil/floor conversions for replay compatibility.
#if RTS_GENERALS /*&& RETAIL_COMPATIBLE_CRC*/
#define REAL_TO_INT_CEIL(x)				(fast_float2long_round(ceilf(x)))
#define REAL_TO_INT_FLOOR(x)			(fast_float2long_round(floorf(x)))
#else
#define REAL_TO_INT_CEIL(x)				(fast_float2long_round(fast_float_ceil(x)))
#define REAL_TO_INT_FLOOR(x)			(fast_float2long_round(fast_float_floor(x)))
#endif

#define FAST_REAL_TRUNC(x)        fast_float_trunc(x)
#define FAST_REAL_CEIL(x)         fast_float_ceil(x)
#define FAST_REAL_FLOOR(x)        fast_float_floor(x)

//--------------------------------------------------------------------
// Derived type definitions
//--------------------------------------------------------------------

// NOTE: Keep these derived types simple, and avoid constructors and destructors
// so they can be used within unions.

// real-valued range defined by low and high values
struct RealRange
{
	Real lo, hi;							// low and high values of the range

	void zero()
	{
		lo = 0.0f;
		hi = 0.0f;
	}

	bool is(Real value) const
	{
		return lo == value && hi == value;
	}

	// combine the given range with us such that we now encompass
	// both ranges
	void combine( RealRange &other )
	{
		lo = min( lo, other.lo );
		hi = max( hi, other.hi );
	}
};

struct Coord2D
{
	Real x, y;

	void zero()
	{
		x = 0.0f;
		y = 0.0f;
	}

	bool is(Real value) const
	{
		return x == value && y == value;
	}

	// GeneralsX @refactor fbraz 03/05/2026 Route BaseType length math through shared Sqrt gateway.
	// Upstream reference: Okladnoj, PR #2670
	// https://github.com/TheSuperHackers/GeneralsGameCode/pull/2670
	Real length() const { return (Real)Sqrt( x*x + y*y ); }
	Real lengthSqr() const { return x*x + y*y; }

	void normalize()
	{
		Real len = length();
		if( len != 0 )
		{
			x /= len;
			y /= len;
		}
	}

	Real toAngle() const;  ///< turn 2D vector into angle (where angle 0 is down the +x axis)

	void add( const Coord2D &a )
	{
		x += a.x;
		y += a.y;
	}

	void sub( const Coord2D &a )
	{
		x -= a.x;
		y -= a.y;
	}

	void operator+=( const Coord2D &a )
	{
		add(a);
	}

	void operator-=( const Coord2D &a )
	{
		sub(a);
	}

	void set( const Coord2D &a )
	{
		x = a.x;
		y = a.y;
	}

	void set( Real ax, Real ay )
	{
		x = ax;
		y = ay;
	}
};

inline Coord2D operator+( const Coord2D &a, const Coord2D &b )
{
	Coord2D c = a;
	c.add(b);
	return c;
}

inline Coord2D operator-( const Coord2D &a, const Coord2D &b )
{
	Coord2D c = a;
	c.sub(b);
	return c;
}

inline Real Coord2D::toAngle() const
{
#if RTS_GENERALS /*&& RETAIL_COMPATIBLE_CRC*/
	Coord2D vector;

	vector.x = x;
	vector.y = y;

	Real dist = (Real)Sqrt(vector.x * vector.x + vector.y * vector.y);

	// normalize
	if (dist == 0.0f)
		return 0.0f;

	Coord2D dir;
	dir.x = 1.0f;
	dir.y = 0.0f;

	Real distInv = 1.0f / dist;
	vector.x *= distInv;
	vector.y *= distInv;

	// dot of two unit vectors is cos of angle
	Real c = dir.x*vector.x + dir.y*vector.y;

	// bound it in case of numerical error
	if (c < -1.0)
		c = -1.0;
	else if (c > 1.0)
		c = 1.0;

	Real value = (Real)ACos( (Real)c );

	// Determine sign by checking Z component of dir cross vector
	// Note this is assumes 2D, and is identical to dotting the perpendicular of v with dir
	Real perpZ = dir.x * vector.y - dir.y * vector.x;
	if (perpZ < 0.0f)
		value = -value;

	// note: to make this 3D, 'dir' and 'vector' can be normalized and dotted just as they are
	// to test sign, compute N = dir X vector, then P = N x dir, then S = P . vector, where sign of
	// S is sign of angle - MSB

	return value;
#else
	const Real len = length();
	if (len == 0.0f)
		return 0.0f;

	Real c = x/len;
	// bound it in case of numerical error
	if (c < -1.0f)
		c = -1.0f;
	else if (c > 1.0f)
		c = 1.0f;

	return y < 0.0f ? -ACos(c) : ACos(c);
#endif
}

struct ICoord2D
{
	Int x, y;

	void zero()
	{
		x = 0;
		y = 0;
	}

	bool is(Int value) const
	{
		return x == value && y == value;
	}

	Int length() const { return (Int)Sqrt( (double)(x*x + y*y) ); }
	Int lengthSqr() const { return x*x + y*y; }

	void add( const ICoord2D &a )
	{
		x += a.x;
		y += a.y;
	}

	void sub( const ICoord2D &a )
	{
		x -= a.x;
		y -= a.y;
	}

	void operator+=( const ICoord2D &a )
	{
		add(a);
	}

	void operator-=( const ICoord2D &a )
	{
		sub(a);
	}

	void set( const ICoord2D &a )
	{
		x = a.x;
		y = a.y;
	}

	void set( Int ax, Int ay )
	{
		x = ax;
		y = ay;
	}
};

inline ICoord2D operator+( const ICoord2D &a, const ICoord2D &b )
{
	ICoord2D c = a;
	c.add(b);
	return c;
}

inline ICoord2D operator-( const ICoord2D &a, const ICoord2D &b )
{
	ICoord2D c = a;
	c.sub(b);
	return c;
}

struct Region2D
{
	Coord2D lo, hi;						// bounds of 2D rectangular region

	void zero()
	{
		lo.zero();
		hi.zero();
	}

	bool is(Real value) const
	{
		return lo.is(value) && hi.is(value);
	}

	Real width() const { return hi.x - lo.x; }
	Real height() const { return hi.y - lo.y; }
	Bool isInRegion( Real x, Real y ) const { return (lo.x < x) && (x < hi.x) && (lo.y < y) && (y < hi.y); }
};

struct IRegion2D
{
	ICoord2D lo, hi;					// bounds of 2D rectangular region

	void zero()
	{
		lo.zero();
		hi.zero();
	}

	bool is(Int value) const
	{
		return lo.is(value) && hi.is(value);
	}

	Int width() const { return hi.x - lo.x; }
	Int height() const { return hi.y - lo.y; }
	Bool isInRegion( Int x, Int y ) const { return (lo.x < x) && (x < hi.x) && (lo.y < y) && (y < hi.y); }
};


struct Coord3D
{
	Real x, y, z;

	Real length() const { return (Real)Sqrt( x*x + y*y + z*z ); }
	Real lengthSqr() const { return ( x*x + y*y + z*z ); }

	void normalize()
	{
		Real len = length();

		if( len != 0 )
		{
			x /= len;
			y /= len;
			z /= len;
		}
	}

	static void crossProduct( const Coord3D &a, const Coord3D &b, Coord3D &r )
	{
		r.x = (a.y * b.z - a.z * b.y);
		r.y = (a.z * b.x - a.x * b.z);
		r.z = (a.x * b.y - a.y * b.x);
	}

	void zero()
	{
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}

	bool is(Real value) const
	{
		return x == value && y == value && z == value;
	}

	void add( const Coord3D &a )
	{
		x += a.x;
		y += a.y;
		z += a.z;
	}

	void sub( const Coord3D &a )
	{
		x -= a.x;
		y -= a.y;
		z -= a.z;
	}

	void operator+=( const Coord3D &a )
	{
		add(a);
	}

	void operator-=( const Coord3D &a )
	{
		sub(a);
	}

	void set( const Coord3D &a )
	{
		x = a.x;
		y = a.y;
		z = a.z;
	}

	void set( Real ax, Real ay, Real az )
	{
		x = ax;
		y = ay;
		z = az;
	}

	void scale( Real scale )
	{
		x *= scale;
		y *= scale;
		z *= scale;
	}

	Bool equals( const Coord3D &r )
	{
		return (x == r.x &&
						y == r.y &&
						z == r.z);
	}

	Bool operator==( const Coord3D &r ) const
	{
		return (x == r.x &&
						y == r.y &&
						z == r.z);
	}
};

inline Coord3D operator+( const Coord3D &a, const Coord3D &b )
{
	Coord3D c = a;
	c.add(b);
	return c;
}

inline Coord3D operator-( const Coord3D &a, const Coord3D &b )
{
	Coord3D c = a;
	c.sub(b);
	return c;
}

struct ICoord3D
{
	Int x, y, z;

	Int length() const { return (Int)Sqrt( (double)(x*x + y*y + z*z) ); }
	Int lengthSqr() const { return x*x + y*y + z*z; }

	void zero()
	{
		x = 0;
		y = 0;
		z = 0;
	}

	bool is(Int value) const
	{
		return x == value && y == value && z == value;
	}

	void add( const ICoord3D &a )
	{
		x += a.x;
		y += a.y;
		z += a.z;
	}

	void sub( const ICoord3D &a )
	{
		x -= a.x;
		y -= a.y;
		z -= a.z;
	}

	void operator+=( const ICoord3D &a )
	{
		add(a);
	}

	void operator-=( const ICoord3D &a )
	{
		sub(a);
	}

	void set( const ICoord3D &a )
	{
		x = a.x;
		y = a.y;
		z = a.z;
	}

	void set( Int ax, Int ay, Int az )
	{
		x = ax;
		y = ay;
		z = az;
	}
};

inline ICoord3D operator+( const ICoord3D &a, const ICoord3D &b )
{
	ICoord3D c = a;
	c.add(b);
	return c;
}

inline ICoord3D operator-( const ICoord3D &a, const ICoord3D &b )
{
	ICoord3D c = a;
	c.sub(b);
	return c;
}

// For alternative see AABoxClass
struct Region3D
{
	Coord3D lo, hi;						// axis-aligned bounding box

	Real width() const { return hi.x - lo.x; }
	Real height() const { return hi.y - lo.y; }
	Real depth() const { return hi.z - lo.z; }

	void zero() { lo.zero(); hi.zero(); }

	bool is(Real value) const
	{
		return lo.is(value) && hi.is(value);
	}

	// Set XY from a 2D region and leave Z unchanged.
	void setXY(const Region2D &region)
	{
		lo.x = region.lo.x;
		lo.y = region.lo.y;
		hi.x = region.hi.x;
		hi.y = region.hi.y;
	}

	// Set XY from any amount of points and leave Z unchanged.
	void setXYFromPoints(const Coord3D* points, Int count)
	{
		lo.x = points[0].x;
		lo.y = points[0].y;
		hi.x = points[0].x;
		hi.y = points[0].y;
		for (Int i = 1; i < count; ++i)
		{
			if (points[i].x < lo.x)
				lo.x = points[i].x;
			if (points[i].y < lo.y)
				lo.y = points[i].y;
			if (points[i].x > hi.x)
				hi.x = points[i].x;
			if (points[i].y > hi.y)
				hi.y = points[i].y;
		}
	}

	void setFromPoints(const Coord3D* points, Int count)
	{
		lo = points[0];
		hi = points[0];
		for (Int i = 1; i < count; ++i)
		{
			if (points[i].x < lo.x)
				lo.x = points[i].x;
			if (points[i].y < lo.y)
				lo.y = points[i].y;
			if (points[i].z < lo.z)
				lo.z = points[i].z;
			if (points[i].x > hi.x)
				hi.x = points[i].x;
			if (points[i].y > hi.y)
				hi.y = points[i].y;
			if (points[i].z > hi.z)
				hi.z = points[i].z;
		}
	}

	Bool isInRegionNoZ( const Coord3D &query ) const
	{
		return (lo.x < query.x) && (query.x < hi.x) &&
					 (lo.y < query.y) && (query.y < hi.y);
	}

	Bool isInRegion( const Coord3D &query ) const
	{
		return (lo.x < query.x) && (query.x < hi.x) &&
					 (lo.y < query.y) && (query.y < hi.y) &&
					 (lo.z < query.z) && (query.z < hi.z);
	}
};

struct IRegion3D
{
	ICoord3D lo, hi;					// axis-aligned bounding box

	void zero()
	{
		lo.zero();
		hi.zero();
	}

	bool is(Int value) const
	{
		return lo.is(value) && hi.is(value);
	}

	Int width() const { return hi.x - lo.x; }
	Int height() const { return hi.y - lo.y; }
	Int depth() const { return hi.z - lo.z; }
};


struct RGBColor
{
	Real red, green, blue;		// range between 0 and 1

	Int getAsInt() const
	{
		return
			((Int)(red * 255.0) << 16) |
			((Int)(green * 255.0) << 8) |
			((Int)(blue * 255.0) << 0);
	}

	void setFromInt(Int c)
	{
		red = ((c >> 16) & 0xff) / 255.0f;
		green = ((c >>  8) & 0xff) / 255.0f;
		blue = ((c >>  0) & 0xff) / 255.0f;
	}

};

struct RGBAColorReal
{

	Real red, green, blue, alpha;  // range between 0.0 and 1.0

};

struct RGBAColorInt
{

	UnsignedInt red, green, blue, alpha;  // range between 0 and 255

};
