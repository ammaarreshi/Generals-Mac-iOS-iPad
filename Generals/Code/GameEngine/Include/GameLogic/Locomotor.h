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

// FILE: Locomotor.h /////////////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, Feb 2002
// Desc: Locomotor Descriptions
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/NameKeyGenerator.h"
#include "Common/Override.h"
#include "Common/Snapshot.h"
#include "GameLogic/Damage.h"
#include "GameLogic/LocomotorSet.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Locomotor;
class LocomotorTemplate;
class INI;
class PhysicsBehavior;
enum BodyDamageType CPP_11(: Int);
enum PhysicsTurningType CPP_11(: Int);

// if we ever re-enable jets circling for landing, we need this. so keep in around just in case. (srj)
#define NO_CIRCLE_FOR_LANDING

//-------------------------------------------------------------------------------------------------
enum LocomotorAppearance CPP_11(: Int)
{
	LOCO_LEGS_TWO,
	LOCO_WHEELS_FOUR,
	LOCO_TREADS,
	LOCO_HOVER,
	LOCO_THRUST,
	LOCO_WINGS,
	LOCO_CLIMBER,			// human climber - backs down cliffs.
	LOCO_OTHER,

	LOCOMOTOR_APPEARANCE_COUNT
};

enum LocomotorPriority CPP_11(: Int)
{
	LOCO_MOVES_BACK,				// In a group, this one moves toward the back
	LOCO_MOVES_MIDDLE,			// In a group, this one stays in the middle
	LOCO_MOVES_FRONT,				// In a group, this one moves toward the front of the group

	LOCOMOTOR_PRIORITY_COUNT
};

#ifdef DEFINE_LOCO_APPEARANCE_NAMES
static const char *const TheLocomotorAppearanceNames[] =
{
	"TWO_LEGS",
	"FOUR_WHEELS",
	"TREADS",
	"HOVER",
	"THRUST",
	"WINGS",
	"CLIMBER",
	"OTHER",

	nullptr
};
static_assert(ARRAY_SIZE(TheLocomotorAppearanceNames) == LOCOMOTOR_APPEARANCE_COUNT + 1, "Array size");
#endif

//-------------------------------------------------------------------------------------------------
enum LocomotorBehaviorZ CPP_11(: Int)
{
	Z_NO_Z_MOTIVE_FORCE,							// does whatever physics tells it, but has no z-force of its own.
	Z_SEA_LEVEL,											// keep at surface-of-water level
	Z_SURFACE_RELATIVE_HEIGHT,				// try to follow a specific height relative to terrain/water height
	Z_ABSOLUTE_HEIGHT,								// try follow a specific height regardless of terrain/water height
	Z_FIXED_SURFACE_RELATIVE_HEIGHT,		// stays fixed at surface-rel height, regardless of physics
	Z_FIXED_ABSOLUTE_HEIGHT,						// stays fixed at absolute height, regardless of physics
	Z_RELATIVE_TO_GROUND_AND_BUILDINGS,	// stays fixed at surface-rel height including buildings, regardless of physics
	Z_SMOOTH_RELATIVE_TO_HIGHEST_LAYER,	// try to follow a height relative to the highest layer.

	LOCOMOTOR_BEHAVIOR_Z_COUNT
};

#ifdef DEFINE_LOCO_Z_NAMES
static const char *const TheLocomotorBehaviorZNames[] =
{
	"NO_Z_MOTIVE_FORCE",
	"SEA_LEVEL",
	"SURFACE_RELATIVE_HEIGHT",
	"ABSOLUTE_HEIGHT",
	"FIXED_SURFACE_RELATIVE_HEIGHT",
	"FIXED_ABSOLUTE_HEIGHT",
	"FIXED_RELATIVE_TO_GROUND_AND_BUILDINGS",
	"RELATIVE_TO_HIGHEST_LAYER",

	nullptr
};
static_assert(ARRAY_SIZE(TheLocomotorBehaviorZNames) == LOCOMOTOR_BEHAVIOR_Z_COUNT + 1, "Array size");
#endif

//-------------------------------------------------------------------------------------------------
class LocomotorTemplate : public Overridable
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( LocomotorTemplate, "LocomotorTemplate"  )
	friend class Locomotor;

public:

	LocomotorTemplate();

	/// field table for loading the values from an INI
	const FieldParse* getFieldParse() const;

	void friend_setName(const AsciiString& n) { m_name = n; }

	void validate();

protected:


private:
	/**
		Units check:

		-- Velocity:			dist/frame
		-- Acceleration:	dist/(frame*frame)
		-- Forces:				(mass*dist)/(frame*frame)
	*/
	AsciiString								m_name;
	LocomotorSurfaceTypeMask	m_surfaces;							///< flags indicating the kinds of surfaces we can use
	Real											m_maxSpeed;							///< max speed
	Real											m_maxSpeedDamaged;			///< max speed when "damaged"
	Real											m_minSpeed;							///< we should never brake past this
	Real											m_maxTurnRate;					///< max rate at which we can turn, in rads/frame
	Real											m_maxTurnRateDamaged;		///< max turn rate when "damaged"
	Real											m_acceleration;					///< max acceleration
	Real											m_accelerationDamaged;	///< max acceleration when damaged
	Real											m_lift;									///< max lifting acceleration (flying objects only)
	Real											m_liftDamaged;					///< max lift when damaged
	Real											m_braking;							///< max braking (deceleration)
	Real											m_minTurnSpeed;					///< we must be going >= this speed in order to turn
	Real											m_preferredHeight;			///< our preferred height (if flying)
	Real											m_preferredHeightDamping;		///< how aggressively to adjust to preferred height: 1.0 = very much so, 0.1 = gradually, etc
	Real											m_circlingRadius;				///< for flying things, the radius at which they circle their "maintain" destination. (pos = cw, neg = ccw, 0 = smallest possible)
	Real											m_speedLimitZ;					///< try to avoid going up or down at more than this speed, if possible
	Real											m_extra2DFriction;			///< extra 2dfriction to apply (via Physics)
	Real											m_maxThrustAngle;				///< THRUST locos only: how much we deflect our thrust angle
	LocomotorBehaviorZ				m_behaviorZ;						///< z-axis behavior
	LocomotorAppearance				m_appearance;						///< how we should diddle the Drawable to imitate this motion
	LocomotorPriority					m_movePriority;					///< Where we move - front, middle, back.

	Real											m_accelPitchLimit;			///< Maximum amount we will pitch up or down under acceleration (including recoil.)
	Real											m_bounceKick;						///< How much simulating rough terrain "bounces" a wheel up.
	Real											m_pitchStiffness;				///< How stiff the springs are forward & back.
	Real											m_rollStiffness;				///< How stiff the springs are side to side.
	Real											m_pitchDamping;					///< How good the shock absorbers are.
	Real											m_rollDamping;					///< How good the shock absorbers are.
	Real											m_pitchByZVelCoef;			///< How much we pitch in response to z-speed.
	Real											m_thrustRoll;						///< Thrust roll around X axis
	Real											m_wobbleRate;						///< how fast thrust things "wobble"
	Real											m_minWobble;						///< how much thrust things "wobble"
	Real											m_maxWobble;						///< how much thrust things "wobble"
	Real											m_forwardVelCoef;				///< How much we pitch in response to speed.
	Real											m_lateralVelCoef;				///< How much we roll in response to speed.
	Real											m_forwardAccelCoef;			///< How much we pitch in response to acceleration.
	Real											m_lateralAccelCoef;			///< How much we roll in response to acceleration.
	Real											m_uniformAxialDamping;	///< For Attenuating the pitch and roll rates
	Real											m_turnPivotOffset;			///< should we pivot around noncenter? (-1.0 = rear, 0.0 = center, 1.0 = front)
	Int												m_airborneTargetingHeight;	///< The height transition at witch I should mark myself as a AA target.

	Real											m_closeEnoughDist;			///< How close we have to approach the end of a path before stopping
	Bool											m_isCloseEnoughDist3D;	///< And is that calculation 3D, for very rare cases that need to move straight down.
	Real											m_ultraAccurateSlideIntoPlaceFactor;			///< how much we can fudge turning when ultra-accurate

	Bool											m_locomotorWorksWhenDead;	///< should locomotor continue working even when object is "dead"?
	Bool											m_allowMotiveForceWhileAirborne;	///< can we apply motive when airborne?
	Bool											m_apply2DFrictionWhenAirborne;	// apply "2d friction" even when airborne... useful for realistic-looking movement
	Bool											m_downhillOnly;	// pinewood derby, moves only by gravity pulling downhill
	Bool											m_stickToGround;				// if true, can't leave ground
	Bool											m_canMoveBackward;				// if true, can move backwards.
	Bool											m_hasSuspension;				///< If true, calculate 4 wheel independent suspension values.
	Real											m_maximumWheelExtension; ///< Maximum distance wheels can move down.  (negative value)
	Real											m_maximumWheelCompression; ///< Maximum distance wheels can move up.  (positive value)
	Real											m_wheelTurnAngle;				///< How far the front wheels can turn.

	// Fields for wander locomotor
	Real											m_wanderWidthFactor;
	Real											m_wanderLengthFactor;
	Real											m_wanderAboutPointRadius;
};

typedef OVERRIDE<LocomotorTemplate> LocomotorTemplateOverride;

// ---------------------------------------------------------
class Locomotor : public MemoryPoolObject, public Snapshot
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(Locomotor, "Locomotor" )

	friend class LocomotorStore;

public:

	void setPhysicsOptions(Object* obj);

	void locoUpdate_moveTowardsPosition(Object* obj, const Coord3D& goalPos,
		Real onPathDistToGoal, Real desiredSpeed, Bool *blocked);
	void locoUpdate_moveTowardsAngle(Object* obj, Real angle);
	/**
		Kill any current (2D) velocity (but stay at current position, or as close as possible)

		return true if we can maintain the position without being called every frame (eg, we are
		resting on the ground), false if not (eg, we are hovering or circling)
	*/
	Bool locoUpdate_maintainCurrentPosition(Object* obj);

	Real getMaxSpeedForCondition(BodyDamageType condition) const;  ///< get max speed given condition
	Real getMaxTurnRate(BodyDamageType condition) const;  ///< get max turning rate given condition
	Real getMaxAcceleration(BodyDamageType condition) const;  ///< get acceleration given condition
	Real getMaxLift(BodyDamageType condition) const;  ///< get acceleration given condition
	Real getBraking() const;  ///< get braking given condition

	Real getPreferredHeight() const { return m_preferredHeight;} ///< Just return preferredheight, no damage consideration
	void restorePreferredHeightFromTemplate() { m_preferredHeight = m_template->m_preferredHeight; };
	Real getPreferredHeightDamping() const { return m_preferredHeightDamping;}
	LocomotorAppearance getAppearance() const { return m_template->m_appearance; }
	LocomotorPriority getMovePriority() const { return m_template->m_movePriority; }
	LocomotorSurfaceTypeMask getLegalSurfaces() const { return m_template->m_surfaces; }

	AsciiString getTemplateName() const { return m_template->m_name;}
	Real getMinSpeed() const { return m_template->m_minSpeed;}
	Real getAccelPitchLimit() const { return m_template->m_accelPitchLimit;}	///< Maximum amount we will pitch up or down under acceleration (including recoil.)
	Real getBounceKick() const { return m_template->m_bounceKick;}						///< How much simulating rough terrain "bounces" a wheel up.
	Real getPitchStiffness() const { return m_template->m_pitchStiffness;}			///< How stiff the springs are forward & back.
	Real getRollStiffness() const { return m_template->m_rollStiffness;}				///< How stiff the springs are side to side.
	Real getPitchDamping() const { return m_template->m_pitchDamping;}	///< How good the shock absorbers are.
	Real getRollDamping() const { return m_template->m_rollDamping;}	///< How good the shock absorbers are.
	Real getPitchByZVelCoef() const { return m_template->m_pitchByZVelCoef;}		///< How much we pitch in response to speed.
	Real getThrustRoll() const { return m_template->m_thrustRoll; }  ///< Thrust roll
	Real getWobbleRate() const { return m_template->m_wobbleRate; }  ///< how fast thrust things "wobble"
	Real getMaxWobble() const { return m_template->m_maxWobble; }  ///< how much thrust things "wobble"
	Real getMinWobble() const { return m_template->m_minWobble; }  ///< how much thrust things "wobble"

	Real getForwardVelCoef() const { return m_template->m_forwardVelCoef;}		///< How much we pitch in response to speed.
	Real getLateralVelCoef() const { return m_template->m_lateralVelCoef;}			///< How much we roll in response to speed.
	Real getForwardAccelCoef() const { return m_template->m_forwardAccelCoef;}		///< How much we pitch in response to acceleration.
	Real getLateralAccelCoef() const { return m_template->m_lateralAccelCoef;}			///< How much we roll in response to acceleration.
	Real getUniformAxialDamping() const { return m_template->m_uniformAxialDamping;}			///< How much we roll in response to acceleration.
	Real getTurnPivotOffset() const { return m_template->m_turnPivotOffset;}
	Bool getApply2DFrictionWhenAirborne() const { return m_template->m_apply2DFrictionWhenAirborne; }
	Bool getIsDownhillOnly() const { return m_template->m_downhillOnly; }
	Bool getAllowMotiveForceWhileAirborne() const { return m_template->m_allowMotiveForceWhileAirborne; }
	Int getAirborneTargetingHeight() const { return m_template->m_airborneTargetingHeight; }
	Bool getLocomotorWorksWhenDead() const { return m_template->m_locomotorWorksWhenDead; }
	Bool getStickToGround() const { return m_template->m_stickToGround; }
	Real getCloseEnoughDist() const { return m_closeEnoughDist; }
	Bool isCloseEnoughDist3D() const { return getFlag(IS_CLOSE_ENOUGH_DIST_3D); }
	Bool hasSuspension() const {return m_template->m_hasSuspension;}
	Bool canMoveBackwards() const {return m_template->m_canMoveBackward;}
	Real getMaxWheelExtension() const {return m_template->m_maximumWheelExtension;}
	Real getMaxWheelCompression() const {return m_template->m_maximumWheelCompression;}
	Real getWheelTurnAngle() const {return m_template->m_wheelTurnAngle;}

	Real getWanderWidthFactor() const {return m_template->m_wanderWidthFactor;}
	Real getWanderAboutPointRadius() const {return m_template->m_wanderAboutPointRadius;}

	Real calcMinTurnRadius(BodyDamageType condition, Real* timeToTravelThatDist) const;

	/// this is handy for doing things like forcing helicopters to crash realistically: cut their lift.
	void setMaxLift(Real lift) { m_maxLift = lift; }
	void setMaxSpeed(Real speed)
	{
		DEBUG_ASSERTCRASH(!(speed <= 0.0f && m_template->m_appearance == LOCO_THRUST), ("THRUST locos may not have zero speeds!"));
		m_maxSpeed = speed;
	}
	void setMaxAcceleration(Real accel) { m_maxAccel = accel; }
	void setMaxBraking(Real braking) { m_maxBraking = braking; }
	void setMaxTurnRate(Real turn) { m_maxTurnRate = turn; }
	void setAllowInvalidPosition(Bool allow) { setFlag(ALLOW_INVALID_POSITION, allow); }
	void setCloseEnoughDist( Real dist ) { m_closeEnoughDist = dist; }
	void setCloseEnoughDist3D( Bool setting ) { setFlag(IS_CLOSE_ENOUGH_DIST_3D, setting); }

	void setPreferredHeight( Real height ) { m_preferredHeight = height; }

#ifdef CIRCLE_FOR_LANDING
	/**
		if we are climbing/diving more than this, circle as needed rather
		than just diving or climbing directly. (only useful for Winged things)
	*/
	inline void setAltitudeChangeThresholdForCircling(Real a) { m_circleThresh = a; }
#endif

	/**
		when off (the default), things get to adjust their z-pos as their
		loco says (in particular, airborne things tend to try to fly at a preferred height).

		when on, they do their best to reach the specified zpos, even if it's not at their preferred height.
		this is used mainly for force missiles to swoop in on their target, and to force airplane takeoff/landing
		to go smoothly.
	*/
	void setUsePreciseZPos(Bool u) { setFlag(PRECISE_Z_POS, u); }

	/**
    when off (the default), units slow down as they approach their target.

    when on, units go full speed till the end, and may overshoot their target.
    this is useful mainly in some weird, temporary situations where we know we are
    going to follow this move with another one...	or for carbombs.
	*/
	void setNoSlowDownAsApproachingDest(Bool u) { setFlag(NO_SLOW_DOWN_AS_APPROACHING_DEST, u); }

	/**
    when off (the default), units do their normal stuff.

    when on, we cheat and make very precise motion, regardless of loco settings.
    this is accomplished by cranking up the unit's turning rate, friction, lift (for airborne things),
    and possibly other things. This is useful mainly when doing maneuvers where precision
    is VITAL, such as airplane takeoff/landing.

		For ground units, it also allows units to have a destination off of a pathfing grid.

	*/
	void setUltraAccurate(Bool u) { setFlag(ULTRA_ACCURATE, u); }
	Bool isUltraAccurate() const { return getFlag(ULTRA_ACCURATE); }

	Bool isMovingBackwards() const {return getFlag(MOVING_BACKWARDS);}

	void startMove(); ///< Indicates that a move is starting, primarily to reset the donut timer. jba.

protected:
	void moveTowardsPositionLegs(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed);
	void moveTowardsPositionLegsWander(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed);
	void moveTowardsPositionClimb(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed);
	void moveTowardsPositionWheels(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed);
	void moveTowardsPositionTreads(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed);
	void moveTowardsPositionOther(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed);
	void moveTowardsPositionHover(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed);
	void moveTowardsPositionThrust(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed);
	void moveTowardsPositionWings(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed);

	void maintainCurrentPositionThrust(Object* obj, PhysicsBehavior *physics);
	void maintainCurrentPositionOther(Object* obj, PhysicsBehavior *physics);
	void maintainCurrentPositionLegs(Object* obj, PhysicsBehavior *physics) { maintainCurrentPositionOther(obj, physics); }
	void maintainCurrentPositionWheels(Object* obj, PhysicsBehavior *physics) { maintainCurrentPositionOther(obj, physics); }
	void maintainCurrentPositionTreads(Object* obj, PhysicsBehavior *physics) { maintainCurrentPositionOther(obj, physics); }
	void maintainCurrentPositionHover(Object* obj, PhysicsBehavior *physics);
	void maintainCurrentPositionWings(Object* obj, PhysicsBehavior *physics);

	PhysicsTurningType rotateTowardsPosition(Object* obj, const Coord3D& goalPos, Real *relAngle=nullptr);

	/*
		return true if we can maintain the position without being called every frame (eg, we are
		resting on the ground), false if not (eg, we are hovering or circling)
	*/
	Bool handleBehaviorZ(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos);
	PhysicsTurningType rotateObjAroundLocoPivot(Object* obj, const Coord3D& goalPos, Real maxTurnRate, Real *relAngle = nullptr);

	Real getSurfaceHtAtPt(Real x, Real y);
	Real calcLiftToUseAtPt(Object* obj, PhysicsBehavior *physics, Real curZ, Real surfaceAtPt, Real preferredHeight);

	Bool fixInvalidPosition(Object* obj, PhysicsBehavior *physics);

protected:
	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

protected:

	Locomotor(const LocomotorTemplate* tmpl);

	// Note, "Law of the Big Three" applies here
	//Locomotor(); -- nope, we don't have a default ctor. (srj)
	Locomotor(const Locomotor& that);
	Locomotor& operator=(const Locomotor& that);
	//~Locomotor();

private:

	//
	// Note: these values are saved in save files, so you MUST NOT REMOVE OR CHANGE
	// existing values!
	//
	enum LocoFlag
	{
		IS_BRAKING = 0,
		ALLOW_INVALID_POSITION,
		MAINTAIN_POS_IS_VALID,
		PRECISE_Z_POS,
		NO_SLOW_DOWN_AS_APPROACHING_DEST,
		OVER_WATER,					// To allow things to move slower/faster over water and do special effects
		ULTRA_ACCURATE,
		MOVING_BACKWARDS,				// If we are moving backwards.
		DOING_THREE_POINT_TURN,	// If we are doing a 3 pt turn.
		CLIMBING,								// If we are in the process of climbing.
		IS_CLOSE_ENOUGH_DIST_3D,
		OFFSET_INCREASING
	};

	Bool getFlag(LocoFlag f) const { return (m_flags & (1 << f)) != 0; }
	void setFlag(LocoFlag f, Bool b) { if (b) m_flags |= (1<<f); else m_flags &= ~(1<<f); }

	LocomotorTemplateOverride m_template;		///< the kind of Locomotor this is
	Coord3D			m_maintainPos;
	Real				m_brakingFactor;
	Real				m_maxLift;
	Real				m_maxSpeed;
	Real				m_maxAccel;
	Real				m_maxBraking;
	Real				m_maxTurnRate;
	Real				m_closeEnoughDist;
#ifdef CIRCLE_FOR_LANDING
	Real				m_circleThresh;
#endif
	UnsignedInt	m_flags;
	Real				m_preferredHeight;
	Real				m_preferredHeightDamping;

	Real				m_angleOffset;
	Real				m_offsetIncrement;
	UnsignedInt m_donutTimer;				///< Frame time to keep units from doing the donut. jba.


};

//-------------------------------------------------------------------------------------------------
/**
	The "store" used to hold all the LocomotorTemplates in existence. This is usually used when creating
	an Object, but can be used at any time after that. (It is explicitly
	OK to swap an Object's Locomotor out at any given time.)
*/
//-------------------------------------------------------------------------------------------------
class LocomotorStore : public SubsystemInterface
{
public:

	LocomotorStore();
	virtual ~LocomotorStore() override;

	virtual void init() override { };
	virtual void reset() override;
	virtual void update() override;

	/**
		Find the LocomotorTemplate with the given name. If no such LocomotorTemplate exists, return null.
	*/
	const LocomotorTemplate* findLocomotorTemplate(NameKeyType namekey) const;
	LocomotorTemplate* findLocomotorTemplate(NameKeyType namekey);

	Locomotor* newLocomotor(const LocomotorTemplate *tmpl) const
	{
		return newInstance(Locomotor)(tmpl);	// my, that was easy
	}

	// locoTemplate is who we're overriding
	LocomotorTemplate *newOverride(LocomotorTemplate *locoTemplate);


	static void parseLocomotorTemplateDefinition(INI* ini);

protected:

private:

	typedef std::map< NameKeyType, LocomotorTemplate*, std::less<NameKeyType>/**/> LocomotorTemplateMap;

	LocomotorTemplateMap m_locomotorTemplates;

};

// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////
extern LocomotorStore *TheLocomotorStore;
