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

// FILE: GenerateMinefieldBehavior.cpp ///////////////////////////////////////////////////////////////////////
// Author:
// Desc:
///////////////////////////////////////////////////////////////////////////////////////////////////


// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#define DEFINE_SLOWDEATHPHASE_NAMES

#include "Common/GlobalData.h"
#include "Common/Thing.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/RandomValue.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/GenerateMinefieldBehavior.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Weapon.h"


//-------------------------------------------------------------------------------------------------
GenerateMinefieldBehaviorModuleData::GenerateMinefieldBehaviorModuleData()
{
	m_mineName.clear();
	m_genFX = nullptr;
	m_distanceAroundObject = TheGlobalData->m_standardMinefieldDistance;
	m_minesPerSquareFoot = TheGlobalData->m_standardMinefieldDensity;
	m_onDeath = false;
	m_borderOnly = true;
	m_alwaysCircular = false;
	m_smartBorder = false;
	m_smartBorderSkipInterior = true;
	m_randomJitter = 0.0f;
	m_skipIfThisMuchUnderStructure = 0.33f;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void GenerateMinefieldBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{

	static const FieldParse dataFieldParse[] =
	{
		{ "MineName", INI::parseAsciiString,	nullptr, offsetof( GenerateMinefieldBehaviorModuleData, m_mineName ) },
		{ "GenerationFX", INI::parseFXList,	nullptr, offsetof( GenerateMinefieldBehaviorModuleData, m_genFX ) },
		{ "DistanceAroundObject", INI::parseReal, nullptr, offsetof( GenerateMinefieldBehaviorModuleData, m_distanceAroundObject ) },
		{ "MinesPerSquareFoot", INI::parseReal, nullptr, offsetof( GenerateMinefieldBehaviorModuleData, m_minesPerSquareFoot ) },
		{ "GenerateOnlyOnDeath", INI::parseBool, nullptr, offsetof(GenerateMinefieldBehaviorModuleData, m_onDeath) },
		{ "BorderOnly", INI::parseBool, nullptr, offsetof(GenerateMinefieldBehaviorModuleData, m_borderOnly) },
		{ "SmartBorder", INI::parseBool, nullptr, offsetof(GenerateMinefieldBehaviorModuleData, m_smartBorder) },
		{ "SmartBorderSkipInterior", INI::parseBool, nullptr, offsetof(GenerateMinefieldBehaviorModuleData, m_smartBorderSkipInterior) },
		{ "AlwaysCircular", INI::parseBool, nullptr, offsetof(GenerateMinefieldBehaviorModuleData, m_alwaysCircular) },
		{ "RandomJitter", INI::parsePercentToReal, nullptr, offsetof(GenerateMinefieldBehaviorModuleData, m_randomJitter) },
		{ "SkipIfThisMuchUnderStructure", INI::parsePercentToReal, nullptr, offsetof(GenerateMinefieldBehaviorModuleData, m_skipIfThisMuchUnderStructure) },
		{ nullptr, nullptr, nullptr, 0 }
	};

  BehaviorModuleData::buildFieldParse(p);
  p.add(dataFieldParse);
  p.add(UpgradeMuxData::getFieldParse(), offsetof( GenerateMinefieldBehaviorModuleData, m_upgradeMuxData ));
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GenerateMinefieldBehavior::GenerateMinefieldBehavior( Thing *thing, const ModuleData* moduleData ) : BehaviorModule( thing, moduleData )
{
	m_target.zero();
	m_generated = false;
	m_hasTarget = false;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
GenerateMinefieldBehavior::~GenerateMinefieldBehavior()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void GenerateMinefieldBehavior::upgradeImplementation()
{
	placeMines();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void GenerateMinefieldBehavior::onDie( const DamageInfo *damageInfo )
{
	const GenerateMinefieldBehaviorModuleData* d = getGenerateMinefieldBehaviorModuleData();

	if (d->m_onDeath)
	{
		placeMines();
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void GenerateMinefieldBehavior::setMinefieldTarget(const Coord3D* pos)
{
	if (pos)
	{
		m_hasTarget = true;
		m_target = *pos;
	}
	else
	{
		m_hasTarget = false;
		m_target.zero();
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
const Coord3D* GenerateMinefieldBehavior::getMinefieldTarget() const
{
	return m_hasTarget ? &m_target : getObject()->getPosition();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static Bool isAnythingTooClose2D(const std::vector<Object*>& v, const Coord3D& pos, Real minDistSqr)
{
	for (std::vector<Object*>::const_iterator it = v.begin(); it != v.end(); ++it)
	{
		const Coord3D* p = (*it)->getPosition();
		Real distSqr = sqr(p->x - pos.x) + sqr(p->y - pos.y);
		if (distSqr < minDistSqr)
			return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static void offsetBySmallRandomAmount(Coord3D& pt, Real maxAmt)
{
	pt.x += GameLogicRandomValueReal(-maxAmt, maxAmt);
	pt.y += GameLogicRandomValueReal(-maxAmt, maxAmt);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Object* GenerateMinefieldBehavior::placeMineAt(const Coord3D& pt, const ThingTemplate* mineTemplate, Team* team, const Object* producer)
{
	Coord3D tmp = pt;
	tmp.z = 99999.0f;
	PathfindLayerEnum layer = TheTerrainLogic->getHighestLayerForDestination(&tmp);

	if (layer == LAYER_GROUND && TheTerrainLogic->isUnderwater(pt.x, pt.y))
		return nullptr;

	if (layer == LAYER_GROUND && TheTerrainLogic->isCliffCell(pt.x, pt.y))
		return nullptr;

	Real orient = GameLogicRandomValueReal(-PI, PI);

	// if the mine will be "mostly" under a structure, don't place it.
	// for now, "mostly" means "central third of radius would overlap"
	const GenerateMinefieldBehaviorModuleData* d = getGenerateMinefieldBehaviorModuleData();
	GeometryInfo geom = mineTemplate->getTemplateGeometryInfo();
	Real mineRadius = mineTemplate->getTemplateGeometryInfo().getBoundingCircleRadius();
	geom.expandFootprint(mineRadius * -(1.0f - d->m_skipIfThisMuchUnderStructure));
	ObjectIterator *iter = ThePartitionManager->iteratePotentialCollisions( &pt, geom, orient );
	MemoryPoolObjectHolder hold(iter);
	for (Object* them = iter->first(); them; them = iter->next())
	{
		if (them->isKindOf(KINDOF_STRUCTURE))
			return nullptr;
	}

	Object* mine = TheThingFactory->newObject(mineTemplate, team);
	mine->setPosition(&pt);
	mine->setOrientation(orient);
	mine->setProducer(producer);

	for (BehaviorModule** bmi = mine->getBehaviorModules(); *bmi; ++bmi)
	{
		LandMineInterface* lmi = (*bmi)->getLandMineInterface();
		if (lmi)
		{
			lmi->setScootParms(*producer->getPosition(), pt);
			break;
		}
	}

	return mine;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void GenerateMinefieldBehavior::placeMinesAlongLine(const Coord3D& posStart, const Coord3D& posEnd, const ThingTemplate* mineTemplate, Bool skipOneAtStart)
{
	const Object* obj = getObject();
	const GenerateMinefieldBehaviorModuleData* d = getGenerateMinefieldBehaviorModuleData();
	Team* team = obj->getControllingPlayer()->getDefaultTeam();

	Real dx = posEnd.x - posStart.x;
	Real dy = posEnd.y - posStart.y;
	Real len = WWMath::SqrtOrigin(sqr(dx) + sqr(dy));
	Real mineRadius = mineTemplate->getTemplateGeometryInfo().getBoundingCircleRadius();
	Real mineDiameter = mineRadius * 2.0f;
	Real mineJitter = mineRadius*d->m_randomJitter;
	Int numMines = REAL_TO_INT_CEIL(len / mineDiameter);
	if (numMines < 1)
		numMines = 1;
	Real inc = len/numMines;
	for (Real place = skipOneAtStart ? inc : 0; place <= len; place += inc)
	{
		Coord3D pt;
		pt.x = posStart.x + place * dx / len;
		pt.y = posStart.y + place * dy / len;
		pt.z = TheTerrainLogic->getGroundHeight( pt.x, pt.y );
		offsetBySmallRandomAmount(pt, mineJitter);
		placeMineAt(pt, mineTemplate, team, obj);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static void makeCorner(const Coord3D& pos, Real majorRadius, Real minorRadius, const Matrix3D& mtx, Coord3D& corner)
{
	Vector3 tmp;
	tmp.X = majorRadius;
	tmp.Y = minorRadius;
	tmp.Z = 0;
	Matrix3D::Transform_Vector(mtx, tmp, &tmp);
	corner.x = tmp.X;
	corner.y = tmp.Y;
	corner.z = tmp.Z;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void GenerateMinefieldBehavior::placeMinesAroundRect(const Coord3D& pos, Real majorRadius, Real minorRadius, const ThingTemplate* mineTemplate)
{
	const Object* obj = getObject();
	const Matrix3D* mtx = obj->getTransformMatrix();

	Coord3D pt[4];
	makeCorner(pos,  majorRadius,  minorRadius, *mtx, pt[0]);
	makeCorner(pos, -majorRadius,  minorRadius, *mtx, pt[1]);
	makeCorner(pos, -majorRadius, -minorRadius, *mtx, pt[2]);
	makeCorner(pos,  majorRadius, -minorRadius, *mtx, pt[3]);

	placeMinesAlongLine(pt[0], pt[1], mineTemplate, true);
	placeMinesAlongLine(pt[1], pt[2], mineTemplate, true);
	placeMinesAlongLine(pt[2], pt[3], mineTemplate, true);
	placeMinesAlongLine(pt[3], pt[0], mineTemplate, true);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void GenerateMinefieldBehavior::placeMinesAroundCircle(const Coord3D& pos, Real radius, const ThingTemplate* mineTemplate)
{
	const Object* obj = getObject();
	const GenerateMinefieldBehaviorModuleData* d = getGenerateMinefieldBehaviorModuleData();
	Team* team = obj->getControllingPlayer()->getDefaultTeam();

	Real circum = 2.0f * PI * radius;
	Real mineRadius = mineTemplate->getTemplateGeometryInfo().getBoundingCircleRadius();
	Real mineDiameter = mineRadius * 2.0f;
	Real mineJitter = mineRadius*d->m_randomJitter;
	Int numMines = REAL_TO_INT_CEIL(circum / mineDiameter);
	if (numMines < 1)
		numMines = 1;
	Real angleInc = (2*PI)/numMines;
	Real angleLim = (2*PI) - angleInc*0.5f;
	for (Real angle = 0; angle < angleLim; angle += angleInc)
	{
		Coord3D pt;
		pt.x = pos.x + radius * Cos(angle);
		pt.y = pos.y + radius * Sin(angle);
		pt.z = TheTerrainLogic->getGroundHeight( pt.x, pt.y );
		offsetBySmallRandomAmount(pt, mineJitter);
		placeMineAt(pt, mineTemplate, team, obj);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void GenerateMinefieldBehavior::placeMinesInFootprint(const GeometryInfo& geom, const ThingTemplate* mineTemplate)
{
	const Object* obj = getObject();
	const GenerateMinefieldBehaviorModuleData* d = getGenerateMinefieldBehaviorModuleData();
	Team* team = obj->getControllingPlayer()->getDefaultTeam();

	Real area = geom.getFootprintArea();
	Int numMines = REAL_TO_INT_CEIL(d->m_minesPerSquareFoot * area);
	if (numMines < 1)
		numMines = 1;

	const Coord3D* target = getMinefieldTarget();
	std::vector<Object*> minesCreatedSoFar;
	Real minDistSqr = sqr(mineTemplate->getTemplateGeometryInfo().getBoundingCircleRadius() * 2.0f);
	for (int i = 0; i < numMines; ++i)
	{
		Coord3D pt;
		Int maxRetry = 100;
		do
		{
			geom.makeRandomOffsetWithinFootprint(pt);
			pt.x += target->x;
			pt.y += target->y;
			pt.z += target->z;
			--maxRetry;
		} while (isAnythingTooClose2D(minesCreatedSoFar, pt, minDistSqr) && maxRetry > 0);
		DEBUG_ASSERTCRASH(maxRetry>0,("ran out of retries %f",minDistSqr));

		if (getObject()->getGeometryInfo().isPointInFootprint(*target, pt))
			continue;

		Object* mine = placeMineAt(pt, mineTemplate, team, obj);	// can return null.
		if (mine)
			minesCreatedSoFar.push_back(mine);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void GenerateMinefieldBehavior::placeMines()
{
	if (m_generated)
		return;

	m_generated = true;

	const Object* obj = getObject();
	const GenerateMinefieldBehaviorModuleData* d = getGenerateMinefieldBehaviorModuleData();
	const ThingTemplate* mineTemplate = TheThingFactory->findTemplate(d->m_mineName);
	if (!mineTemplate)
	{
		DEBUG_CRASH(("mine %s not found",d->m_mineName.str()));
		return;
	}

	const Coord3D* target = getMinefieldTarget();
	if (d->m_smartBorder)
	{
		GeometryInfo geom = obj->getGeometryInfo();

		if (!d->m_smartBorderSkipInterior)
		{
			geom = mineTemplate->getTemplateGeometryInfo();
			placeMineAt(*target, mineTemplate, obj->getControllingPlayer()->getDefaultTeam(), obj);
		}

		if (d->m_alwaysCircular)
			geom.set(GEOMETRY_CYLINDER, false, 1, geom.getBoundingCircleRadius(), geom.getBoundingCircleRadius());

		Real mineRadius = mineTemplate->getTemplateGeometryInfo().getBoundingCircleRadius();
		Real mineDiameter = mineRadius * 2.0f;
		geom.expandFootprint(mineRadius);
		do
		{
			if (geom.getGeomType() == GEOMETRY_BOX && !d->m_alwaysCircular)
			{
				placeMinesAroundRect(*target, geom.getMajorRadius(), geom.getMinorRadius(), mineTemplate);
			}
			else
			{
				placeMinesAroundCircle(*target, geom.getMajorRadius(), mineTemplate);
			}
			geom.expandFootprint(mineDiameter);
		} while (geom.getBoundingCircleRadius() < d->m_distanceAroundObject);
	}
	else if (d->m_borderOnly)
	{
		GeometryInfo geom = obj->getGeometryInfo();
		geom.expandFootprint(d->m_distanceAroundObject);

		if (geom.getGeomType() == GEOMETRY_BOX && !d->m_alwaysCircular)
		{
			placeMinesAroundRect(*target, geom.getMajorRadius(), geom.getMinorRadius(), mineTemplate);
		}
		else
		{
			placeMinesAroundCircle(*target, geom.getMajorRadius(), mineTemplate);
		}
	}
	else
	{
		GeometryInfo geom = obj->getGeometryInfo();
		geom.expandFootprint(d->m_distanceAroundObject);

		if (d->m_alwaysCircular)
			geom.set(GEOMETRY_CYLINDER, false, 1, geom.getBoundingCircleRadius(), geom.getBoundingCircleRadius());

		placeMinesInFootprint(geom, mineTemplate);
	}

	FXList::doFXObj(d->m_genFX, obj);

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void GenerateMinefieldBehavior::crc( Xfer *xfer )
{

	// extend base class
	BehaviorModule::crc( xfer );

	// extend base class
	UpgradeMux::upgradeMuxCRC( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void GenerateMinefieldBehavior::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// base class
	BehaviorModule::xfer( xfer );

	// mux "base class"
	UpgradeMux::upgradeMuxXfer( xfer );

	// generated
	xfer->xferBool( &m_generated );
	xfer->xferBool( &m_hasTarget );
	xfer->xferCoord3D( &m_target );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void GenerateMinefieldBehavior::loadPostProcess()
{

	// extend base class
	BehaviorModule::loadPostProcess();

	// extend base class
	UpgradeMux::upgradeMuxLoadPostProcess();

}
