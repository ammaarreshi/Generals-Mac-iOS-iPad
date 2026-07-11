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

// View.h /////////////////////////////////////////////////////////////////////////////////////////
// A "view", or window, into the World
// Author: Michael S. Booth, February 2001
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/GameType.h"
#include "Common/Snapshot.h"
#include "Lib/BaseType.h"
#include "WW3D2/camera.h"
#include "WW3D2/coltype.h"			///< we don't generally do this, but we need the W3D collision types
#include "WWMath/plane.h"
#include "WWMath/wwmath.h"

#define DEFAULT_VIEW_WIDTH 640
#define DEFAULT_VIEW_HEIGHT 480
#define DEFAULT_VIEW_ORIGIN_X 0
#define DEFAULT_VIEW_ORIGIN_Y 0

// FORWARD DECLARATIONS ///////////////////////////////////////////////////////////////////////////
class Drawable;
class ViewLocation;
class Thing;
class Waypoint;
class LookAtTranslator;
enum FilterTypes CPP_11(: Int);
enum FilterModes CPP_11(: Int);

// ------------------------------------------------------------------------------------------------
constexpr const Real ViewDefaultPitchRadians = DEG_TO_RADF(37.5f);
constexpr const Real ViewDefaultLowPitchRadians = DEG_TO_RADF(37.0f);
constexpr const Real ViewDefaultYawRadians = DEG_TO_RADF(0.0f);
constexpr const Real ViewDefaultMaxHeightAboveTerrain = 310.0f;

// ------------------------------------------------------------------------------------------------
enum PickType CPP_11(: Int)
{
	PICK_TYPE_TERRAIN						= COLL_TYPE_0,
	PICK_TYPE_SELECTABLE				= COLL_TYPE_1,
	PICK_TYPE_SHRUBBERY					= COLL_TYPE_2,
	PICK_TYPE_MINES							= COLL_TYPE_3,	// mines aren't normally selectable, but workers/dozers need to
	PICK_TYPE_FORCEATTACKABLE		= COLL_TYPE_4,
	PICK_TYPE_ALL_DRAWABLES			= (PICK_TYPE_SELECTABLE | PICK_TYPE_SHRUBBERY | PICK_TYPE_MINES | PICK_TYPE_FORCEATTACKABLE)
};

// ------------------------------------------------------------------------------------------------
/** The implementation of common view functionality. */
// ------------------------------------------------------------------------------------------------
class View : public Snapshot
{

public:

	enum
	{
		ZoomHeightPerSecond = 10,
	};

	/// Add an impulse force to shake the camera
	enum CameraShakeType
	{
		SHAKE_SUBTLE = 0,
		SHAKE_NORMAL,
		SHAKE_STRONG,
		SHAKE_SEVERE,
		SHAKE_CINE_EXTREME,		//Added for cinematics ONLY
		SHAKE_CINE_INSANE,		//Added for cinematics ONLY
		SHAKE_COUNT
	};

  // Return values for worldToScreenTriReturn
  enum WorldToScreenReturn CPP_11(: Int)
  {
    WTS_INSIDE_FRUSTUM = 0, // On the screen (inside frustum of camera)
    WTS_OUTSIDE_FRUSTUM,    // Return is valid but off the screen (outside frustum of camera)
    WTS_INVALID,            // No transform possible
  };

public:

	View();
	virtual ~View();

	virtual void init();
	virtual void reset();
	virtual UnsignedInt getID() { return m_id; }

	virtual void setZoomLimited( Bool limit ) { m_zoomLimited = limit; }			///< limit the zoom height
	virtual Bool isZoomLimited() const { return m_zoomLimited; }							///< get status of zoom limit

	/// pick drawable given the screen pixel coords.  If force attack, picks bridges as well.
	virtual Drawable *pickDrawable( const ICoord2D *screen, Bool forceAttack, PickType pickType ) = 0;

	/// all drawables in the 2D screen region will call the 'callback'
	virtual Int iterateDrawablesInRegion( IRegion2D *screenRegion,
																				Bool (*callback)( Drawable *draw, void *userData ),
																				void *userData ) = 0;

	/// Project the 4 corners of this view into the world and return each point as a parameter,
	/// the world points are at the requested Z. Returns whether all corner view rays intersect
	/// with the Z plane.
	virtual PlaneClass::IntersectionResType getScreenCornerWorldPointsAtZ( Coord3D *topLeft, Coord3D *topRight,
																							Coord3D *bottomRight, Coord3D *bottomLeft,
																							Real z, ViewportClass viewPort = ViewportClass() );

	virtual void setWidth( Int width ) { m_width = width; }
	virtual Int getWidth() { return m_width; }
	virtual void setHeight( Int height ) { m_height = height; }
	virtual Int getHeight() { return m_height; }
	virtual void setOrigin( Int x, Int y) { m_originX=x; m_originY=y;}				///< Sets location of top-left view corner on display
	virtual void getOrigin( Int *x, Int *y) { *x=m_originX; *y=m_originY;}			///< Return location of top-left view corner on display

	virtual void forceRedraw() = 0;

	virtual void lookAt( const Coord3D *o );														///< Center the view on the given coordinate
	virtual void initHeightForMap() {};														///< Init the camera height for the map at the current position.
	virtual void resetPivotToGround() {};													///< Set the camera pivot to the terrain height at the current position.
	virtual void scrollBy( const Coord2D *delta );														///< Shift the view by the given delta

	virtual void moveCameraTo(const Coord3D *o, Int frames, Int shutter, Bool orient, Real easeIn=0.0f, Real easeOut=0.0f) { lookAt( o ); }
	virtual void moveCameraAlongWaypointPath(Waypoint *way, Int frames, Int shutter, Bool orient, Real easeIn=0.0f, Real easeOut=0.0f) { }
	virtual Bool isCameraMovementFinished() { return TRUE; }
	virtual void cameraModFinalZoom(Real finalZoom, Real easeIn=0.0f, Real easeOut=0.0f){}; ///< Final zoom for current camera movement.
	virtual void cameraModRollingAverage(Int framesToAverage){}; ///< Number of frames to average movement for current camera movement.
	virtual void cameraModFinalTimeMultiplier(Int finalMultiplier){}; ///< Final time multiplier for current camera movement.
	virtual void cameraModFinalPitch(Real finalPitch, Real easeIn=0.0f, Real easeOut=0.0f){};	 ///< Final pitch for current camera movement.
	virtual void cameraModFreezeTime(){ }					///< Freezes time during the next camera movement.
	virtual void cameraModFreezeAngle(){ }					///< Freezes time during the next camera movement.
	virtual void cameraModLookToward(Coord3D *pLoc){}			///< Sets a look at point during camera movement.
	virtual void cameraModFinalLookToward(Coord3D *pLoc){}			///< Sets a look at point during camera movement.
	virtual void cameraModFinalMoveTo(Coord3D *pLoc){ };			///< Sets a final move to.

	// (gth) C&C3 animation controlled camera feature
	virtual void cameraEnableSlaveMode(const AsciiString & thingtemplateName, const AsciiString & boneName) {}
	virtual void cameraDisableSlaveMode() {}
	virtual	void Add_Camera_Shake(const Coord3D & position,float radius, float duration, float power) {}
	virtual FilterModes getViewFilterMode() {return (FilterModes)0;}			///< Turns on viewport special effect (black & white mode)
	virtual FilterTypes getViewFilterType() {return (FilterTypes)0;}			///< Turns on viewport special effect (black & white mode)
	virtual Bool setViewFilterMode(FilterModes filterMode) { return FALSE; }			///< Turns on viewport special effect (black & white mode)
	virtual void setViewFilterPos(const Coord3D *pos) { };			///<  Passes a position to the special effect filter.
	virtual Bool setViewFilter( FilterTypes filter) { return FALSE;}			///< Turns on viewport special effect (black & white mode)

	virtual void setFadeParameters(Int fadeFrames, Int direction) { };
	virtual void set3DWireFrameMode(Bool enable) { };

 	virtual void resetCamera(const Coord3D *location, Int frames, Real easeIn=0.0f, Real easeOut=0.0f) {}; ///< Move camera to location, and reset to default angle & zoom.
 	virtual void rotateCamera(Real rotations, Int frames, Real easeIn=0.0f, Real easeOut=0.0f) {}; ///< Rotate camera about current viewpoint.
	virtual void rotateCameraTowardObject(ObjectID id, Int milliseconds, Int holdMilliseconds, Real easeIn=0.0f, Real easeOut=0.0f) {};	///< Rotate camera to face an object, and hold on it
	virtual void rotateCameraTowardPosition(const Coord3D *pLoc, Int milliseconds, Real easeIn=0.0f, Real easeOut=0.0f, Bool reverseRotation=FALSE) {};	///< Rotate camera to face a location.
	virtual Bool isTimeFrozen(){ return false;}					///< Freezes time during the next camera movement.
	virtual Int	 getTimeMultiplier() {return 1;};				///< Get the time multiplier.
	virtual void setTimeMultiplier(Int multiple) {}; ///< Set the time multiplier.
	virtual void setCameraHeightAboveGroundLimitsToDefault(Real heightScale = 1.0f) {};
	virtual void setDefaultView(Real pitch, Real angle, Real maxHeight) {};
	virtual void zoomCamera( Real finalZoom, Int milliseconds, Real easeIn=0.0f, Real easeOut=0.0f ) {};
	virtual void pitchCamera( Real finalPitch, Int milliseconds, Real easeIn=0.0f, Real easeOut=0.0f ) {};

	virtual Bool isDoingScriptedCamera() = 0;
	virtual void stopDoingScriptedCamera() = 0;

	virtual void setAngle( Real radians );															///< Rotate the view around the vertical axis to the given angle (yaw)
	virtual Real getAngle() { return m_angle; }										///< Return current camera angle
	virtual Real getDefaultAngle() { return m_defaultAngle; }			///< Return current default camera angle
	virtual void setPitch( Real radians );															///< Rotate the view around the horizontal axis to the given angle (pitch)
	virtual Real getPitch() { return m_pitch; }										///< Return current camera pitch
	virtual void setDefaultPitch( Real radians );												///< Set new default camera pitch. It affects the camera distance to the ground
	virtual Real getDefaultPitch() { return m_defaultPitch; }						///< Return current default camera pitch
	virtual void setAngleToDefault();															///< Set the view angle back to default
	virtual void setPitchToDefault();															///< Set the view pitch back to default
	void setPosition( const Coord3D &pos ) { m_pos = pos; }
	void setPosition2D( const Coord2D &pos ) { m_pos.x = pos.x; m_pos.y = pos.y; }
	const Coord3D &getPosition() const { return m_pos; } ///< Returns position camera is looking at
	Coord2D getPosition2D() const { Coord2D c = { m_pos.x, m_pos.y }; return c; } ///< Returns position camera is looking at

	virtual Coord3D get3DCameraPosition() const { Coord3D c={0,0,0}; return c; } ///< Returns the actual camera position
	virtual Coord3D get3DCameraDirection() const { Coord3D c={0,0,0}; return c; } ///< Returns the actual camera view direction
	virtual void set3DCameraLookAt(const Coord3D &pos, const Coord3D &dir, Real roll) {} ///< Set the actual camera position and view direction

	virtual Real getZoom() { return m_zoom; }
	virtual void setZoom(Real z) { m_zoom = z; }
	virtual Real getHeightAboveGround() { return m_heightAboveGround; }
	virtual void setHeightAboveGround(Real z);
	virtual void zoom( Real height ); ///< Zoom in/out, closer to the ground, limit to min, or farther away from the ground, limit to max
	virtual void setZoomToMax();
	virtual void setZoomToDefault() { m_zoom  = 1.0f; } ///< Set zoom to default value
	virtual void setOkToAdjustHeight( Bool val ) { m_okToAdjustHeight = val; }	///< Set this to adjust camera height

	// TheSuperHackers @info Functions to call for user camera controls, not by the scripted camera.
	Bool userSetPosition(const Coord3D &pos)             { return doUserAction(&View::setPosition, pos); }
	Bool userSetAngle(Real radians)                      { return doUserAction(&View::setAngle, radians); }
	Bool userSetAngleToDefault()                         { return doUserAction(&View::setAngleToDefault); }
	Bool userSetPitch(Real radians)                      { return doUserAction(&View::setPitch, radians); }
	Bool userSetDefaultPitch(Real radians)               { return doUserAction(&View::setDefaultPitch, radians); }
	Bool userSetPitchToDefault()                         { return doUserAction(&View::setPitchToDefault); }
	Bool userZoom(Real height)                           { return doUserAction(&View::zoom, height); }
	Bool userSetZoom(Real z)                             { return doUserAction(&View::setZoom, z); }
	Bool userSetZoomToDefault()                          { return doUserAction(&View::setZoomToDefault); }
	Bool userSetFieldOfView(Real angle)                  { return doUserAction(&View::setFieldOfView, angle); }
	Bool userLookAt(const Coord3D *o)                    { return doUserAction(&View::lookAt, o); }
	Bool userResetPivotToGround()                        { return doUserAction(&View::resetPivotToGround); }
	Bool userScrollBy(const Coord2D *delta)              { return doUserAction(&View::scrollBy, delta); }
	Bool userSetLocation(const ViewLocation *location)   { return doUserAction(&View::setLocation, location); }
	Bool userSetCameraLock(ObjectID id)                  { return doUserAction(&View::setCameraLock, id); }
	Bool userSetCameraLockDrawable(Drawable *drawable)   { return doUserAction(&View::setCameraLockDrawable, drawable); }

	void lockUserControlUntilFrame(UnsignedInt frame) { m_userControlLockedUntilFrame = frame; } ///< Locks the user control over camera until the given frame is reached.

	virtual void setUserControlled(Bool value) { m_isUserControlled = value; }
	Bool isUserControlLocked() const;

	// for debugging
	virtual Real getTerrainHeightAtPivot() { return m_terrainHeightAtPivot; }
	virtual Real getCurrentHeightAboveGround() { return m_currentHeightAboveGround; }

	virtual void setFieldOfView( Real angle ) { m_FOV = angle; }				///< Set the horizontal field of view angle
	virtual Real getFieldOfView() { return m_FOV; }								///< Get the horizontal field of view angle

  Bool worldToScreen( const Coord3D *w, ICoord2D *s ) { return worldToScreenTriReturn( w, s ) == WTS_INSIDE_FRUSTUM; }	///< Transform world coordinate "w" into screen coordinate "s"
  virtual WorldToScreenReturn worldToScreenTriReturn(const Coord3D *w, ICoord2D *s ) = 0; ///< Like worldToScreen(), but with a more informative return value

	/// Transform screen point to the viewed world position on the 3D terrain. Returns true when intersection exists.
	virtual Bool screenToTerrain( const ICoord2D *screen, Coord3D *world ) = 0;

	/// Transform screen point to the viewed world position at the specified world Z height. Returns the type of the intersection.
	virtual PlaneClass::IntersectionResType screenToWorldAtZ( const ICoord2D *screen, Coord3D *world, Real z ) = 0;

	virtual void getLocation ( ViewLocation *location );								///< write the view's current location in to the view location object
	virtual void setLocation ( const ViewLocation *location );					///< set the view's current location from to the view location object

	virtual void drawView() = 0;															///< Render the world visible in this view.
	virtual void updateView() = 0;					///<called once per frame to determine the final camera and object transforms
	virtual void stepView() = 0; ///< Update view for every fixed time step

	virtual ObjectID getCameraLock() const { return m_cameraLock; }
	virtual void setCameraLock(ObjectID id) { m_cameraLock = id; m_lockDist = 0.0f; m_lockType = LOCK_FOLLOW; }
	virtual void snapToCameraLock() { m_snapImmediate = TRUE; }
	enum CameraLockType { LOCK_FOLLOW, LOCK_TETHER };
	virtual void setSnapMode( CameraLockType lockType, Real lockDist ) { m_lockType = lockType; m_lockDist = lockDist; }

	virtual Drawable *getCameraLockDrawable() const { return m_cameraLockDrawable; }
	virtual void setCameraLockDrawable(Drawable *drawable) { m_cameraLockDrawable = drawable; m_lockDist = 0.0f; }

	virtual void setMouseLock( Bool mouseLocked ) { m_mouseLocked = mouseLocked; }					///< lock/unlock the mouse input to the tactical view
	virtual Bool isMouseLocked() { return m_mouseLocked; }														///< is the mouse input locked to the tactical view?

	/// Add an impulse force to shake the camera
	virtual void shake( const Coord3D *epicenter, CameraShakeType shakeType ) { };

	virtual Real getFXPitch() const { return 1.0f; }					///< returns the FX pitch angle
	virtual void forceCameraAreaConstraintRecalc() {}
	virtual void setGuardBandBias( const Coord2D *gb ) = 0;

protected:

	friend class Display;

	// snapshot methods
	virtual void crc( Xfer *xfer ) override { }
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override { }

	virtual View *prependViewToList( View *list );							///< Prepend this view to the given list, return the new list
	virtual View *getNextView() { return m_next; }				///< Return next view in the set

private:

	template<typename Function>
	Bool doUserAction(Function function)
	{
		if (isUserControlLocked())
			return false;
		stopDoingScriptedCamera();
		setUserControlled(true);
		(this->*function)();
		return true;
	}

	template<typename Function, typename Arg1>
	Bool doUserAction(Function function, Arg1 arg1)
	{
		if (isUserControlLocked())
			return false;
		stopDoingScriptedCamera();
		setUserControlled(true);
		(this->*function)(arg1);
		return true;
	}

protected:

	View *m_next;																								///< List links used by the Display class

	UnsignedInt m_id;																						///< The ID of this view
	static UnsignedInt m_idNext;																///< Used for allocating view ID's for all views

	UnsignedInt m_userControlLockedUntilFrame;									///< Locks the user control over camera until the given frame is reached
	Bool m_isUserControlled;																		///< True if the user moved the camera last, false if the scripted camera moved the camera last

	Coord3D m_pos;																							///< Pivot of the camera, in world coordinates
	Int m_width, m_height;																			///< Dimensions of the view
	Int m_originX, m_originY;																		///< Location of top/left view corner

	Real m_angle;																								///< Angle at which view has been rotated about the Z axis. Expected normalized
	Real m_pitch;																								///< Rotation of view direction around horizontal (X) axis. Expected normalized

	Real m_maxHeightAboveGround;																///< Highest camera above ground value
	Real m_minHeightAboveGround;																///< Lowest camera above ground value
	Real m_zoom;																								///< Current zoom value
	Real m_heightAboveGround;																		///< User's desired camera height above ground
	Bool m_zoomLimited;																					///< Camera restricted in zoom height
	Real m_defaultAngle;																				///< Expected normalized
	Real m_defaultPitch;																				///< Expected normalized
	Real m_currentHeightAboveGround;														///< Actual camera height above ground, or rather height above ground at default pitch
	Real m_terrainHeightAtPivot;																///< Actual terrain height at camera pivot

	ObjectID m_cameraLock;																			///< if nonzero, id of object that the camera should follow
	Drawable *m_cameraLockDrawable;															///< if nonzero, drawable of object that camera should follow.
	CameraLockType m_lockType;																	///< are we following or just tethering?
	Real m_lockDist;																						///< how far can we be when tethered?

	Real m_FOV;																									///< the current field of view angle
	Bool m_mouseLocked;																					///< is the mouse input locked to the tactical view?

	Bool m_okToAdjustHeight;																		///< Should we attempt to adjust camera height?
	Bool m_snapImmediate;																				///< Should we immediately snap to the object we're following?

	Coord2D m_guardBandBias; ///< Extra beefy margins so huge thins can stay "on-screen"

};

// ------------------------------------------------------------------------------------------------
/** Used to save and restore view position */
// ------------------------------------------------------------------------------------------------
class ViewLocation
{
public:

	ViewLocation()
	{
		m_valid = false;
		m_pos.zero();
		m_angle = 0.0f;
		m_pitch = 0.0f;
		m_zoom = 0.0f;
	}

	Bool isValid() const { return m_valid; }
	const Coord3D& getPosition() const { return m_pos; }
	Real getAngle() const { return m_angle; }
	Real getPitch() const { return m_pitch; }
	Real getZoom() const { return m_zoom; }

	void init(Coord3D pos, Real angle, Real pitch, Real zoom)
	{
		m_valid = true;
		m_pos = pos;
		m_angle = angle;
		m_pitch = pitch;
		m_zoom = zoom;
	}

private:

	Bool m_valid;					///< Is this location valid
	Coord3D m_pos;				///< Position of this view, in world coordinates
	Real m_angle;					///< Angle at which view has been rotated about the Z axis
	Real m_pitch;					///< Angle at which view has been rotated about the Y axis
	Real m_zoom;					///< Current zoom value
};

// TheSuperHackers @feature bobtista 31/01/2026
// View that does nothing. Used for Headless Mode.
class ViewDummy : public View
{
public:
	virtual Drawable *pickDrawable( const ICoord2D *screen, Bool forceAttack, PickType pickType ) override
	{
		return nullptr;
	}
	virtual Int iterateDrawablesInRegion( IRegion2D *screenRegion, Bool (*callback)( Drawable *draw, void *userData ), void *userData ) override
	{
		return 0;
	}
	virtual void forceRedraw() override {}
	virtual WorldToScreenReturn worldToScreenTriReturn(const Coord3D *w, ICoord2D *s ) override
	{
		return WTS_INVALID;
	}
	virtual Bool screenToTerrain( const ICoord2D *screen, Coord3D *world ) override { return false; }
	virtual PlaneClass::IntersectionResType screenToWorldAtZ( const ICoord2D *screen, Coord3D *world, Real z ) override { return PlaneClass::NO_INTERSECTION; }
	virtual void drawView() override {}
	virtual void updateView() override {}
	virtual void stepView() override {}
	virtual void setGuardBandBias( const Coord2D *gb ) override {}
	virtual Bool isDoingScriptedCamera() override { return false; }
	virtual void stopDoingScriptedCamera() override {}

	// Do not override View::xfer(). The base implementation must run to serialize valid view state for save file compatibility.
};

// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////
extern View *TheTacticalView;		///< the main tactical interface to the game world
