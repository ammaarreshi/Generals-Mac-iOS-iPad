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

// FILE: Thing.cpp ////////////////////////////////////////////////////////////
// Created:   Colin Day, May 2001
//
// Desc:      Things are the base class for objects and drawables, objects
//						are logic side representations while drawables are client
//						side.  Common data will be held in the Thing defined here
//						and systems that need to work with both of them will work with
//						"Things"
//
//-----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/PerfTimer.h"
#include "Common/Thing.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Lib/trig.h"
#include "GameLogic/TerrainLogic.h"


static constexpr const Real InitialThingPosX = 0.0f;
static constexpr const Real InitialThingPosY = 0.0f;

//=============================================================================
/** Constructor */
//=============================================================================
Thing::Thing( const ThingTemplate *thingTemplate )
{
	// sanity
	if( thingTemplate == nullptr )
	{

		// cannot create thing without template
		DEBUG_CRASH(( "no template" ));
		return;

	}

	m_template = thingTemplate;
#if defined(RTS_DEBUG)
	m_templateName = thingTemplate->getName();
#endif
	m_transform.Make_Identity();
	m_cachedPos.x = InitialThingPosX;
	m_cachedPos.y = InitialThingPosY;
	m_cachedPos.z = 0.0f;
	m_cachedAngle = 0.0f;
	m_cachedDirVector.zero();
	m_cachedAltitudeAboveTerrain = 0;
	m_cachedAltitudeAboveTerrainOrWater = 0;
	m_cacheFlags = 0;

}

//=============================================================================
/** Destructor */
//=============================================================================
Thing::~Thing()
{
}

//DECLARE_PERF_TIMER(ThingMatrixStuff)

//=============================================================================
const ThingTemplate *Thing::getTemplate() const
{
	return m_template;
}

//=============================================================================
Bool Thing::isPositioned() const
{
	return m_cachedPos.x != InitialThingPosX || m_cachedPos.y != InitialThingPosY;
}

//=============================================================================
const Coord3D* Thing::getUnitDirectionVector2D() const
{
	//USE_PERF_TIMER(ThingMatrixStuff)
	if (!(m_cacheFlags & VALID_DIRVECTOR))
	{
		Real angle = getOrientation();
		m_cachedDirVector.x = Cos( angle );
		m_cachedDirVector.y = Sin( angle );
		m_cachedDirVector.z = 0;
		m_cacheFlags |= VALID_DIRVECTOR;
	}

	return &m_cachedDirVector;
}

//=============================================================================
void Thing::getUnitDirectionVector2D(Coord3D& dir) const
{
	dir = *getUnitDirectionVector2D();
}

//=============================================================================
void Thing::getUnitDirectionVector3D(Coord3D& dir) const
{
	Vector3 vdir = m_transform.Get_X_Vector();
	vdir.Normalize();
	dir.x = vdir.X;
	dir.y = vdir.Y;
	dir.z = vdir.Z;
}

//=============================================================================
// the nice thing about this is that we don't have to recalc out cached terrain stuff.
void Thing::setPositionZ( Real z )
{
	//USE_PERF_TIMER(ThingMatrixStuff)
	if( !m_template->isKindOf( KINDOF_STICK_TO_TERRAIN_SLOPE) )
	{
		Real oldAngle = m_cachedAngle;
		Coord3D oldPos = m_cachedPos;
		Matrix3D oldMtx = m_transform;

		m_transform.Set_Z_Translation( z );
		m_cachedPos.z = z;

		if (m_cacheFlags & VALID_ALTITUDE_TERRAIN)
		{
			m_cachedAltitudeAboveTerrain += (z - oldPos.z);
		}
		if (m_cacheFlags & VALID_ALTITUDE_SEALEVEL)
		{
			m_cachedAltitudeAboveTerrainOrWater += (z - oldPos.z);
		}

		reactToTransformChange(&oldMtx, &oldPos, oldAngle);
	}
	else
	{
		Matrix3D mtx;
		const Bool stickToGround = true;	// yes, set the "z" pos
		Coord3D pos = m_cachedPos;
		pos.z = z;
		TheTerrainLogic->alignOnTerrain(getOrientation(), pos, stickToGround, mtx );
		setTransformMatrix(&mtx);
	}
	DEBUG_ASSERTCRASH(!(_isnan(getPosition()->x) || _isnan(getPosition()->y) || _isnan(getPosition()->z)), ("Drawable/Object position NAN! '%s'", m_template->getName().str() ));
}

//=============================================================================
void Thing::setPosition( const Coord3D *pos )
{
	//USE_PERF_TIMER(ThingMatrixStuff)
	if( !m_template->isKindOf( KINDOF_STICK_TO_TERRAIN_SLOPE) )
	{
		Real oldAngle = m_cachedAngle;
		Coord3D oldPos = m_cachedPos;
		Matrix3D oldMtx = m_transform;

		//DEBUG_ASSERTCRASH(!(_isnan(pos->x) || _isnan(pos->y) || _isnan(pos->z)), ("Drawable/Object position NAN! '%s'", m_template->getName().str() ));
		m_transform.Set_X_Translation( pos->x );
		m_transform.Set_Y_Translation( pos->y );
		m_transform.Set_Z_Translation( pos->z );
		m_cachedPos = *pos;
		m_cacheFlags &= ~(VALID_ALTITUDE_TERRAIN | VALID_ALTITUDE_SEALEVEL);	// but don't clear the dir flags.

		reactToTransformChange(&oldMtx, &oldPos, oldAngle);
	}
	else
	{
		Matrix3D mtx;
		const Bool stickToGround = true;	// yes, set the "z" pos
		TheTerrainLogic->alignOnTerrain(getOrientation(), *pos, stickToGround, mtx );
		setTransformMatrix(&mtx);
	}
	DEBUG_ASSERTCRASH(!(_isnan(getPosition()->x) || _isnan(getPosition()->y) || _isnan(getPosition()->z)), ("Drawable/Object position NAN! '%s'", m_template->getName().str() ));
}

//=============================================================================
void Thing::setOrientation( Real angle )
{
	//USE_PERF_TIMER(ThingMatrixStuff)
	Coord3D u, x, y, z, pos;

	// setOrientation always forces us straight up in the Z axis,
	// or aligned with the terrain if we have the magic flag set.
	// don't want this? call setTransformMatrix instead.

	Real oldAngle = m_cachedAngle;
	Coord3D oldPos = m_cachedPos;
	Matrix3D oldMtx = m_transform;

	pos.x = m_transform.Get_X_Translation();
	pos.y = m_transform.Get_Y_Translation();
	pos.z = m_transform.Get_Z_Translation();
	if( m_template->isKindOf( KINDOF_STICK_TO_TERRAIN_SLOPE) )
	{
		Matrix3D mtx;
		const Bool stickToGround = true;	// yes, set the "z" pos
		TheTerrainLogic->alignOnTerrain(angle, pos, stickToGround, m_transform );
	}
	else
	{
		z.x = 0.0f;
		z.y = 0.0f;
		z.z = 1.0f;

		u.x = Cos(angle);
		u.y = Sin(angle);
		u.z = 0.0f;

		y.crossProduct( z, u, y );
		x.crossProduct( y, z, x );

		m_transform.Set(  x.x, y.x, z.x, pos.x,
											x.y, y.y, z.y, pos.y,
											x.z, y.z, z.z, pos.z );
	}

	//DEBUG_ASSERTCRASH(-PI <= angle && angle <= PI, ("Please pass only normalized (-PI..PI) angles to setOrientation (%f).", angle));
	m_cachedAngle = normalizeAngle(angle);
	m_cachedPos = pos;
	m_cacheFlags &= ~VALID_DIRVECTOR;	// but don't clear the altitude flags.

	reactToTransformChange(&oldMtx, &oldPos, oldAngle);
	DEBUG_ASSERTCRASH(!(_isnan(getPosition()->x) || _isnan(getPosition()->y) || _isnan(getPosition()->z)), ("Drawable/Object position NAN! '%s'", m_template->getName().str() ));
}

//=============================================================================
/** Set the world transformation matrix */
//=============================================================================
void Thing::setTransformMatrix( const Matrix3D *mx )
{
	//USE_PERF_TIMER(ThingMatrixStuff)
	Real oldAngle = m_cachedAngle;
	Coord3D oldPos = m_cachedPos;
	Matrix3D oldMtx = m_transform;

	m_transform = *mx;
	m_cachedPos.x = m_transform.Get_X_Translation();
	m_cachedPos.y = m_transform.Get_Y_Translation();
	m_cachedPos.z = m_transform.Get_Z_Translation();
	m_cachedAngle = m_transform.Get_Z_Rotation();
	m_cacheFlags = 0;

	reactToTransformChange(&oldMtx, &oldPos, oldAngle);
	DEBUG_ASSERTCRASH(!(_isnan(getPosition()->x) || _isnan(getPosition()->y) || _isnan(getPosition()->z)), ("Drawable/Object position NAN! '%s'", m_template->getName().str() ));
}

//-------------------------------------------------------------------------------------------------
Bool Thing::isKindOf(KindOfType t) const
{
	return getTemplate()->isKindOf(t);
}

//-------------------------------------------------------------------------------------------------
Bool Thing::isKindOfMulti(const KindOfMaskType& mustBeSet, const KindOfMaskType& mustBeClear) const
{
	return getTemplate()->isKindOfMulti(mustBeSet, mustBeClear);
}

// ------------------------------------------------------------------------------------------------
Bool Thing::isAnyKindOf( const KindOfMaskType& anyKindOf ) const
{
	return getTemplate()->isAnyKindOf( anyKindOf );
}

// ------------------------------------------------------------------------------------------------
Real Thing::calculateHeightAboveTerrain() const
{
	//USE_PERF_TIMER(ThingMatrixStuff)
	const Coord3D* pos = getPosition();
	Real terrainZ = TheTerrainLogic->getGroundHeight( pos->x, pos->y );
	Real myZ = pos->z;
	return myZ - terrainZ;
}

//-------------------------------------------------------------------------------------------------
Real Thing::getHeightAboveTerrain() const
{
	if (!(m_cacheFlags & VALID_ALTITUDE_TERRAIN))
	{
		m_cachedAltitudeAboveTerrain = calculateHeightAboveTerrain();
		m_cacheFlags |= VALID_ALTITUDE_TERRAIN;
	}
	return m_cachedAltitudeAboveTerrain;
}

//-------------------------------------------------------------------------------------------------
Real Thing::getHeightAboveTerrainOrWater() const
{
	//USE_PERF_TIMER(ThingMatrixStuff)
	if (!(m_cacheFlags & VALID_ALTITUDE_SEALEVEL))
	{
		const Coord3D* pos = getPosition();
		Real waterZ;
		if (TheTerrainLogic->isUnderwater(pos->x, pos->y, &waterZ))
		{
			m_cachedAltitudeAboveTerrainOrWater = pos->z - waterZ;
		}
		else
		{
			m_cachedAltitudeAboveTerrainOrWater = getHeightAboveTerrain();
		}
		m_cacheFlags |= VALID_ALTITUDE_SEALEVEL;
	}
	return m_cachedAltitudeAboveTerrainOrWater;
}

//=============================================================================
/** If we treat this as airborne, then they slide down slopes.  This checks whether
they are high enough that we should let them act like they're flying. jba. */
//=============================================================================
Bool Thing::isSignificantlyAboveTerrain() const
{
	// If it's high enough that it will take more than 3 frames to return to the ground,
	// then it's significantly airborne.  jba
	return (getHeightAboveTerrain() > -(3*3)*TheGlobalData->m_gravity);
}


//-------------------------------------------------------------------------------------------------
void Thing::convertBonePosToWorldPos(const Coord3D* bonePos, const Matrix3D* boneTransform, Coord3D* worldPos, Matrix3D* worldTransform) const
{
	if (worldTransform)
	{
#ifdef ALLOW_TEMPORARIES
		*worldTransform = m_transform * (*boneTransform);
#else
		worldTransform->mul(m_transform, *boneTransform);
#endif
	}
	if (worldPos)
	{
		Vector3 vector;
		vector.X = bonePos->x;
		vector.Y = bonePos->y;
		vector.Z = bonePos->z;
		m_transform.Transform_Vector(m_transform, vector, &vector);
		worldPos->x = vector.X;
		worldPos->y = vector.Y;
		worldPos->z = vector.Z;
	}
}

// ------------------------------------------------------------------------------------------------
/** Push the 'in' parameter through our transformation matrix and store in 'out' */
// ------------------------------------------------------------------------------------------------
void Thing::transformPoint( const Coord3D *in, Coord3D *out )
{

	// sanity
	if( in == nullptr || out == nullptr )
		return;

	// for conversion
	Vector3 vectorIn;
	Vector3 vectorOut;

	///@ todo this is dumb and we should not have to convert types
	// convert to Vector3 datatypes
	vectorIn.X = in->x;
	vectorIn.Y = in->y;
	vectorIn.Z = in->z;

	// do the transform
	m_transform.Transform_Vector( m_transform, vectorIn, &vectorOut );

	// store converted vector in 'out'
	out->x = vectorOut.X;
	out->y = vectorOut.Y;
	out->z = vectorOut.Z;

}
