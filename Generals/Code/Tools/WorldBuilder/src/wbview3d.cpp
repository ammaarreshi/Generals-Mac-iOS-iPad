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

// wbview3d.cpp : implementation file
//

#include "StdAfx.h"
#include "resource.h"
#include "wwmath.h"
#include "ww3d.h"
#include "scene.h"
#include "rendobj.h"
#include "camera.h"
#include "intersec.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/Module/W3DModelDraw.h"
#include "agg_def.h"
#include "part_ldr.h"
#include "hanim.h"
#include "dx8wrapper.h"
#include "dx8indexbuffer.h"
#include "dx8vertexbuffer.h"
#include "dx8renderer.h"
#include "dx8fvf.h"
#include "vertmaterial.h"
#include "font3d.h"
#include "render2d.h"
#include "rddesc.h"
#include "textdraw.h"
#include "rect.h"
#include "mesh.h"
#include "meshmdl.h"
#include "line3d.h"
#include "dynamesh.h"
#include "sphereobj.h"
#include "ringobj.h"
#include "surfaceclass.h"
#include "vector2i.h"
#include "bmp2d.h"
#include "decalsys.h"
#include "shattersystem.h"
#include "light.h"
#include "texproject.h"
#include "MapSettings.h"
#include "predlod.h"
#include "SelectMacrotexture.h"
#include "WorldBuilderView.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "MainFrm.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DDynamicLight.h"
#include "WBHeightMap.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "W3DDevice/Common/W3DConvert.h"
#include "W3DDevice/GameClient/W3DShadow.h"
#include "DrawObject.h"
#include "Common/MapObject.h"
#include "Common/GlobalData.h"
#include "ShadowOptions.h"
#include "WorldBuilder.h"
#include "wbview3d.h"
#include "Common/Debug.h"
#include "Common/FramePacer.h"
#include "Common/ThingFactory.h"
#include "GameClient/Water.h"
#include "Common/WellKnownKeys.h"
#include "Common/ThingTemplate.h"
#include "Common/Language.h"
#include "Common/FileSystem.h"
#include "Common/PlayerTemplate.h"
#include "GameLogic/SidesList.h"
#include "GameLogic/TerrainLogic.h"
#include "GameClient/View.h"
#include "GlobalLightOptions.h"
#include "LayersList.h"
#include "ImpassableOptions.h"


#include <d3dx8.h>


// ----------------------------------------------------------------------------
// Misc. Forward Declarations
// ----------------------------------------------------------------------------
class SkeletonSceneClass;

// ----------------------------------------------------------------------------
// Constants:
// ----------------------------------------------------------------------------
#define MAX_LOADSTRING			100
#define WINDOW_WIDTH				640
#define WINDOW_HEIGHT				480
#define UPDATE_TIME					100  /* 10 frames a second */
#define MOUSE_WHEEL_FACTOR	32

#define SAMPLE_DYNAMIC_LIGHT	1
#ifdef SAMPLE_DYNAMIC_LIGHT
static W3DDynamicLight * theDynamicLight = nullptr;
static Real theLightXOffset = 0.1f;
static Real theLightYOffset = 0.07f;
static Int theFlashCount = 0;
#endif

// ----------------------------------------------------------------------------
// Global Variables:
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Static Functions
// ----------------------------------------------------------------------------

static void		WWDebug_Message_Callback(DebugType type, const char * message);
static void		WWAssert_Callback(const char * message);
static void		Debug_Refs();

// ----------------------------------------------------------------------------
static void WWDebug_Message_Callback(DebugType type, const char * message)
{
#ifdef RTS_DEBUG
	::OutputDebugString(message);
	::OutputDebugString("\n");
#endif
}

// ----------------------------------------------------------------------------
static void WWAssert_Callback(const char * message)
{
#ifdef RTS_DEBUG
	::OutputDebugString(message);
	::OutputDebugString("\n");
	::DebugBreak();
#endif
}


// The W3DShadowManager accesses TheTacticalView, so we have to create
// a stub class & object in Worldbuilder for it to access.
class PlaceholderView : public View
{
protected:
	Int m_width, m_height;																			///< Dimensions of the view
	Int m_originX, m_originY;																		///< Location of top/left view corner

protected:
	virtual View *prependViewToList( View *list ) override {return nullptr;};		///< Prepend this view to the given list, return the new list
	virtual View *getNextView() override { return nullptr; }				///< Return next view in the set
public:

	virtual void init() override {};

	virtual UnsignedInt getID() override { return 1; }

	virtual Drawable *pickDrawable( const ICoord2D *screen, Bool forceAttack, PickType pickType ) override {return nullptr;};			///< pick drawable given the screen pixel coords

	/// all drawables in the 2D screen region will call the 'callback'
	virtual Int iterateDrawablesInRegion( IRegion2D *screenRegion,
																				Bool (*callback)( Drawable *draw, void *userData ),
																				void *userData ) override {return 0;};
  virtual WorldToScreenReturn worldToScreenTriReturn( const Coord3D *w, ICoord2D *s ) override { return WTS_INVALID; };	///< Transform world coordinate "w" into screen coordinate "s"
	virtual Bool screenToTerrain( const ICoord2D *screen, Coord3D *world ) override { return false; }
	virtual PlaneClass::IntersectionResType screenToWorldAtZ( const ICoord2D *s, Coord3D *w, Real z ) override { return PlaneClass::NO_INTERSECTION; }
	virtual PlaneClass::IntersectionResType getScreenCornerWorldPointsAtZ( Coord3D *topLeft, Coord3D *topRight,
																							Coord3D *bottomRight, Coord3D *bottomLeft,
																							Real z, ViewportClass viewPort = ViewportClass() ) override { return PlaneClass::NO_INTERSECTION; }

	virtual void drawView() override {};															///< Render the world visible in this view.
	virtual void updateView() override {};															///< Render the world visible in this view.
	virtual void stepView() override {}; ///< Update view for every fixed time step

	virtual void setZoomLimited( Bool limit ) override {}			///< limit the zoom height
	virtual Bool isZoomLimited() const override { return TRUE; }							///< get status of zoom limit

	virtual void setWidth( Int width ) override { m_width = width; }
	virtual Int getWidth() override { return m_width; }
	virtual void setHeight( Int height ) override { m_height = height; }
	virtual Int getHeight() override { return m_height; }
	virtual void setOrigin( Int x, Int y) override { m_originX=x; m_originY=y;}				///< Sets location of top-left view corner on display
	virtual void getOrigin( Int *x, Int *y) override { *x=m_originX; *y=m_originY;}			///< Return location of top-left view corner on display

	virtual void forceRedraw() override { }

	virtual Bool isDoingScriptedCamera() override { return false; }
	virtual void stopDoingScriptedCamera() override {}

	virtual void lookAt( const Coord3D *o ) override {};											///< Center the view on the given coordinate
	virtual void initHeightForMap() override {};												///<  Init the camera height for the map at the current position.
	virtual void scrollBy( Coord2D *delta ){};														///< Shift the view by the given delta
	virtual void moveCameraTo(const Coord3D *o, Int frames, Int shutter,
														Bool orient) {lookAt(o);};
	virtual void moveCameraAlongWaypointPath(Waypoint *way, Int frames, Int shutter,
														Bool orient) {};
	virtual Bool isCameraMovementFinished() override {return true;};
	virtual void resetCamera(const Coord3D *location, Int frames) {}; ///< Move camera to location, and reset to default angle & zoom.
	virtual void rotateCamera(Real rotations, Int frames) {}; ///< Rotate camera about current viewpoint.
	virtual void rotateCameraTowardObject(ObjectID id, Int milliseconds, Int holdMilliseconds) {};	///< Rotate camera to face an object, and hold on it
	virtual void cameraModFinalZoom(Real finalZoom){};			 ///< Final zoom for current camera movement.
	virtual void cameraModRollingAverage(Int framesToAverage) override {}; ///< Number of frames to average movement for current camera movement.
	virtual void cameraModFinalTimeMultiplier(Int finalMultiplier) override {}; ///< Final time multiplier for current camera movement.
	virtual void cameraModFinalPitch(Real finalPitch){};		 ///< Final pitch for current camera movement.
	virtual void cameraModFreezeTime() override {}					///< Freezes time during the next camera movement.
	virtual void cameraModFreezeAngle() override {}					///< Freezes time during the next camera movement.
	virtual void cameraModLookToward(Coord3D *pLoc) override {}			///< Sets a look at point during camera movement.
	virtual void cameraModFinalLookToward(Coord3D *pLoc) override {}			///< Sets a look at point during camera movement.
	virtual void cameraModFinalMoveTo(Coord3D *pLoc) override { };			///< Sets a final move to.
	virtual Bool isTimeFrozen() override { return false;}					///< Freezes time during the next camera movement.
	virtual Int	 getTimeMultiplier() override {return 1;};				///< Get the time multiplier.
	virtual void setTimeMultiplier(Int multiple) override {}; ///< Set the time multiplier.
	virtual void setDefaultView(Real pitch, Real angle, Real maxHeight) override {};
	virtual void zoomCamera( Real finalZoom, Int milliseconds ) {};
	virtual void pitchCamera( Real finalPitch, Int milliseconds ) {};

	virtual void setAngle( Real angle ) override {};																///< Rotate the view around the up axis to the given angle
	virtual Real getAngle() override { return 0; }
	virtual void setPitch( Real angle ) override {};																	///< Rotate the view around the horizontal axis to the given angle
	virtual Real getPitch() override { return 0; }							///< Return current camera pitch
	virtual void setAngleToDefault() override {}											///< Set the view angle back to default
	virtual void setPitchToDefault() override {}											///< Set the view pitch back to default
	virtual void getPosition(Coord3D *pos) {}											///< Return camera position

	virtual Real getHeightAboveGround() override { return 1; }
	virtual void setHeightAboveGround(Real z) override { }
	virtual Real getZoom() override { return 1; }
	virtual void setZoom(Real z) override { }
	virtual void zoomIn() {  }																			///< Zoom in, closer to the ground, limit to min
	virtual void zoomOut() {  }																		///< Zoom out, farther away from the ground, limit to max
	virtual void setZoomToDefault() override { }														///< Set zoom to default value
	virtual Real getMaxZoom() { return 0.0f; }
	virtual void setOkToAdjustHeight( Bool val ) override { }						///< Set this to adjust camera height

	virtual Real getTerrainHeightAtPivot() override { return 0.0f; }
	virtual Real getCurrentHeightAboveGround() override { return 0.0f; }

	virtual void getLocation ( ViewLocation *location ) override {};								///< write the view's current location in to the view location object
	virtual void setLocation ( const ViewLocation *location ) override {};								///< set the view's current location from to the view location object

	virtual ObjectID getCameraLock() const override { return INVALID_ID; }
	virtual void setCameraLock(ObjectID id) override {  }
	virtual void snapToCameraLock() override {  }
	virtual void setSnapMode( CameraLockType lockType, Real lockDist ) override {  }

	virtual Drawable *getCameraLockDrawable() const override { return nullptr; }
	virtual void setCameraLockDrawable(Drawable *drawable) override { }

	virtual void setMouseLock( Bool mouseLocked ) override {}					///< lock/unlock the mouse input to the tactical view
	virtual Bool isMouseLocked() override { return FALSE; }			///< is the mouse input locked to the tactical view?

	virtual void setFieldOfView( Real angle ) override {};							///< Set the horizontal field of view angle
	virtual Real getFieldOfView() override {return 0;};										///< Get the horizontal field of view angle

	virtual Bool setViewFilterMode(enum FilterModes) override {return FALSE;}	///<stub
	virtual void setViewFilterPos(const Coord3D *pos) override {};	///<stub
	virtual void setFadeParameters(Int fadeFrames, Int direction) override {};	///<stub
	virtual void set3DWireFrameMode(Bool enable) override { }; ///<stub
	virtual Bool setViewFilter(		enum FilterTypes m_viewFilterMode) override { return FALSE;}	///<stub
	virtual enum FilterModes	 getViewFilterMode() override {return (enum FilterModes)0;}			///< Turns on viewport special effect (black & white mode)
	virtual enum FilterTypes	 getViewFilterType() override {return (enum FilterTypes)0;}			///< Turns on viewport special effect (black & white mode)

	virtual void shake( const Coord3D *epicenter, CameraShakeType shakeType ) override {};

	virtual Real getFXPitch() const override { return 1.0f; }
	virtual void forceCameraAreaConstraintRecalc() override { }
	virtual void rotateCameraTowardPosition(const Coord3D *pLoc, Int milliseconds, Real easeIn, Real easeOut, Bool reverseRotation) override {};	///< Rotate camera to face an object, and hold on it

	virtual void setGuardBandBias( const Coord2D *gb ) override {};

};

PlaceholderView bogusTacticalView;



// ----------------------------------------------------------------------------
// Customized scene for worldbuilder preview window.
// ----------------------------------------------------------------------------

class SkeletonSceneClass : public RTS3DScene
{
public:
	SkeletonSceneClass() : m_testPass(nullptr) { }
	virtual ~SkeletonSceneClass() override { REF_PTR_RELEASE(m_testPass); }

	void					Set_Material_Pass(MaterialPassClass * pass)	{ REF_PTR_SET(m_testPass, pass); }
	virtual void Remove_Render_Object(RenderObjClass * obj) override;

	Bool safeContains(RenderObjClass *obj);

protected:
	MaterialPassClass *m_testPass;
};


Bool SkeletonSceneClass::safeContains(RenderObjClass *obj)
{
	Bool found = false;
	SceneIterator *sceneIter = Create_Iterator();
	sceneIter->First();
	while(!sceneIter->Is_Done()) {
		if (obj==sceneIter->Current_Item()) {
			found = true;
			break;
		}
		sceneIter->Next();
	}
	Destroy_Iterator(sceneIter);
	return found;
}

// ----------------------------------------------------------------------------

void SkeletonSceneClass::Remove_Render_Object(RenderObjClass * obj)
{
	if (RenderList.Contains(obj)) {
		RenderObjClass *refPtr = nullptr;
		REF_PTR_SET(refPtr, obj); // ref it, as when it gets removed from the scene, may get deleted otherwise.
		RTS3DScene::Remove_Render_Object(obj);
		REF_PTR_RELEASE(refPtr);
	}
}


void WbView3d::setObjTracking(MapObject *pMapObj,  Coord3D pos, Real angle, Bool show)
{
	m_showObjToolTrackingObj = show;
	if (!show) return;
	Real scale;
	AsciiString modelName = getModelNameAndScale(pMapObj, &scale, BODY_PRISTINE);
	if (modelName != m_objectToolTrackingModelName) {
		m_objectToolTrackingModelName = modelName;
		REF_PTR_RELEASE(m_objectToolTrackingObj);
		m_objectToolTrackingObj = m_assetManager->Create_Render_Obj( modelName.str(), scale, 0);
	}
	if (m_objectToolTrackingObj == nullptr) {
		return;
	}
	pos.z += m_heightMapRenderObj->getHeightMapHeight(pos.x, pos.y, nullptr);
	Matrix3D renderObjPos(true);	// init to identity
	renderObjPos.Translate(pos.x, pos.y, pos.z);
	renderObjPos.Rotate_Z(angle);
	m_objectToolTrackingObj->Set_Transform( renderObjPos );
}

// ----------------------------------------------------------------------------
// WbView3d
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
IMPLEMENT_DYNCREATE(WbView3d, WbView)

// ----------------------------------------------------------------------------
WbView3d::WbView3d() :
	m_assetManager(nullptr),
	m_scene(nullptr),
	m_overlayScene(nullptr),
	m_transparentObjectsScene(nullptr),
	m_baseBuildScene(nullptr),
	m_objectToolTrackingObj(nullptr),
	m_showObjToolTrackingObj(false),
	m_camera(nullptr),
	m_heightMapRenderObj(nullptr),
	m_mouseWheelOffset(0),
	m_actualWinSize(0, 0),
	m_cameraAngle(0.0),
	m_FXPitch(1.0f),
	m_actualHeightAboveGround(0.0f),
	m_doPitch(false),
	m_theta(0.0),
	m_time(0),
	m_updateCount(0),
	m_needToLoadRoads(0),
	m_timer(0),
	m_drawObject(nullptr),
	m_layer(nullptr),
	m_buildLayer(nullptr),
	m_intersector(nullptr),
	m_showEntireMap(false),
	m_partialMapSize(129),
	m_showWireframe(false),
	m_projection(false),
	m_showShadows(false),
	m_firstPaint(true),
	m_groundLevel(10),
	m_curTrackingZ(10),
	m_ww3dInited(false),
	m_showLayersList(false),
	m_showMapBoundaries(false)
{
	TheTacticalView = &bogusTacticalView;
	m_actualWinSize.x = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "Width", THREE_D_VIEW_WIDTH);
	m_actualWinSize.y = ::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "Height", THREE_D_VIEW_HEIGHT);
	m_cameraOffset.x = m_cameraOffset.y = m_cameraOffset.z = 1;

	for (Int i=0; i<MAX_GLOBAL_LIGHTS; i++)
	{
		m_globalLight[i] = NEW_REF( LightClass, (LightClass::DIRECTIONAL) );
		m_lightFeedbackMesh[i]=nullptr;
	}

	m_showWireframe = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowWireframe", 0) != 0);
	m_showEntireMap = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowEntireMap", 1) != 0);
	m_projection = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowTopDownView", 0) != 0);
	m_showShadows = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowShadows", 1) != 0);
	TheWritableGlobalData->m_showSoftWaterEdge = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowSoftWater", 1) != 0);
	TheWritableGlobalData->m_use3WayTerrainBlends = (::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowExtraBlends", 1) > 1 ? 2 : 1);
	setShowModels(::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowModels", 1) != 0);
	setShowGarrisoned(::AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowGarrisoned", 0) != 0);
}

// ----------------------------------------------------------------------------
WbView3d::~WbView3d()
{
	for (Int i=0; i<MAX_GLOBAL_LIGHTS; i++)
	{
		if (m_lightFeedbackMesh[i] != nullptr)
		{	m_lightFeedbackMesh[i]->Remove();
			REF_PTR_RELEASE(m_lightFeedbackMesh[i]);
		}
	}

	W3DShaderManager::shutdown();
	shutdownWW3D();
}
// ----------------------------------------------------------------------------
void WbView3d::shutdownWW3D()
{
	if (m_ww3dInited) {
		m_lightList.Reset_List();

		if (m_assetManager) {
			PredictiveLODOptimizerClass::Free();	/// @todo: where does this need to be done?
			m_assetManager->Free_Assets();
			delete m_assetManager;
			m_assetManager = nullptr;
		}

		if (TheW3DShadowManager)
		{
			TheW3DShadowManager->removeAllShadows();
			delete TheW3DShadowManager;
			TheW3DShadowManager=nullptr;
		}
		REF_PTR_RELEASE(m_transparentObjectsScene);
		REF_PTR_RELEASE(m_overlayScene);
		REF_PTR_RELEASE(m_baseBuildScene);
		REF_PTR_RELEASE(m_objectToolTrackingObj);
		REF_PTR_RELEASE(m_scene);
		REF_PTR_RELEASE(m_camera);
		REF_PTR_RELEASE(m_heightMapRenderObj);
		REF_PTR_RELEASE(m_drawObject);
		for (Int i=0; i<MAX_GLOBAL_LIGHTS; i++)
			REF_PTR_RELEASE(m_globalLight[i]);
#ifdef SAMPLE_DYNAMIC_LIGHT
		REF_PTR_RELEASE(theDynamicLight);
#endif
		WW3D::Shutdown();

		WWMath::Shutdown();
	}
	m_ww3dInited = false;

	delete m_intersector;
	m_intersector = nullptr;

	delete m_layer;
	m_layer = nullptr;

	delete m_buildLayer;
	m_buildLayer = nullptr;

	if (m3DFont) {
		m3DFont->Release();
		m3DFont = nullptr;
	}
}

//=============================================================================
// WbView3d::ReleaseResources
//=============================================================================
/** Releases all w3d assets, to prepare for Reset device call. */
//=============================================================================
void WbView3d::ReleaseResources()
{
	if (TheTerrainRenderObject) {
		TheTerrainRenderObject->ReleaseResources();
	}
	if (m3DFont) {
		m3DFont->Release();
	}
	m3DFont = nullptr;
}

//=============================================================================
// WbView3d::ReAcquireResources
//=============================================================================
/** Reallocates all W3D assets after a reset.. */
//=============================================================================
void WbView3d::ReAcquireResources()
{
	if (TheTerrainRenderObject) {
		TheTerrainRenderObject->ReAcquireResources();
		TheTerrainRenderObject->loadRoadsAndBridges(nullptr,FALSE);
		TheTerrainRenderObject->worldBuilderUpdateBridgeTowers( m_assetManager, m_scene );
	}
	IDirect3DDevice8* pDev = DX8Wrapper::_Get_D3D_Device8();
	if (pDev) {

//		CDC* pDC = GetDC();
		LOGFONT logFont;
		logFont.lfHeight = 20;
		logFont.lfWidth = 0;
		logFont.lfEscapement = 0;
		logFont.lfOrientation = 0;
		logFont.lfWeight = FW_REGULAR;
		logFont.lfItalic = FALSE;
		logFont.lfUnderline = FALSE;
		logFont.lfStrikeOut = FALSE;
		logFont.lfCharSet = ANSI_CHARSET;
		logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
		logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
		logFont.lfQuality = DEFAULT_QUALITY;
		logFont.lfPitchAndFamily = DEFAULT_PITCH;
		strcpy(logFont.lfFaceName, "Arial");

		HFONT hFont = CreateFontIndirect(&logFont);
		if (hFont) {
			D3DXCreateFont(pDev, hFont, &m3DFont);
			DeleteObject(hFont);
		} else {
			m3DFont = nullptr;
		}

	} else {
		m3DFont = nullptr;
	}

}

// ----------------------------------------------------------------------------
void WbView3d::killTheTimer()
{
	if (m_timer != 0) {
		KillTimer(m_timer);
		m_timer = 0;
	}
}

// ----------------------------------------------------------------------------
void WbView3d::reset3dEngineDisplaySize(Int width, Int height)
{
	if (m_actualWinSize.x == width &&
		m_actualWinSize.y == height) {
		return;
	}
	bogusTacticalView.setWidth(width);
	bogusTacticalView.setHeight(height);
	bogusTacticalView.setOrigin(0,0);
	m_actualWinSize.x = width;
	m_actualWinSize.y = height;
	if (m_ww3dInited) {
		WW3D::Set_Device_Resolution(m_actualWinSize.x, m_actualWinSize.y, true);
	}
}

// ----------------------------------------------------------------------------
void WbView3d::initAssets()
{

	m_assetManager = new W3DAssetManager;
	m_assetManager->Register_Prototype_Loader(&_ParticleEmitterLoader );
	m_assetManager->Register_Prototype_Loader(&_AggregateLoader);
	m_assetManager->Set_WW3D_Load_On_Demand(true);
}

// ----------------------------------------------------------------------------
#define TERRAIN_SAMPLE_SIZE 40.0f
static Real getHeightAroundPos(WBHeightMap *heightMap, Real x, Real y)
{
	Real terrainHeight = heightMap->getHeightMapHeight(x, y, nullptr);

	// find best approximation of max terrain height we can see
	Real terrainHeightMax = terrainHeight;
	terrainHeightMax = max(terrainHeightMax, heightMap->getHeightMapHeight(x+TERRAIN_SAMPLE_SIZE, y-TERRAIN_SAMPLE_SIZE, nullptr));
	terrainHeightMax = max(terrainHeightMax, heightMap->getHeightMapHeight(x-TERRAIN_SAMPLE_SIZE, y-TERRAIN_SAMPLE_SIZE, nullptr));
	terrainHeightMax = max(terrainHeightMax, heightMap->getHeightMapHeight(x+TERRAIN_SAMPLE_SIZE, y+TERRAIN_SAMPLE_SIZE, nullptr));
	terrainHeightMax = max(terrainHeightMax, heightMap->getHeightMapHeight(x-TERRAIN_SAMPLE_SIZE, y+TERRAIN_SAMPLE_SIZE, nullptr));

	return terrainHeightMax;
}

// ----------------------------------------------------------------------------
void WbView3d::setupCamera()
{
	Matrix3D camtransform(1);
	float zOffset = - m_mouseWheelOffset / 1200;
	Real zoom = 1.0f;
	if (zOffset != 0) {
		Real zPos = (m_cameraOffset.length()-m_groundLevel)/m_cameraOffset.length();
		Real zAbs = zOffset + zPos;
		if (zAbs<0) zAbs = -zAbs;
		if (zAbs<0.01) zAbs = 0.01f;
		//DEBUG_LOG(("zOffset = %.2f, zAbs = %.2f, zPos = %.2f", zOffset, zAbs, zPos));
		if (zOffset > 0) {
			zOffset *= zAbs;
		}	else if (zOffset < -0.3f) {
			zOffset = -0.15f + zOffset/2.0f;
		}
		if (zOffset < -0.6f) {
			zOffset = -0.3f + zOffset/2.0f;
		}
		//DEBUG_LOG(("zOffset = %.2f", zOffset));
		zoom = zAbs;
	}

/////////////////////////////////////////////////////////////////
	Vector3 sourcePos, targetPos;

	Real angle = m_cameraAngle;
	Real pitch = 0;
	Coord3D pos;
	pos.x = m_centerPt.X* MAP_XY_FACTOR;
	pos.y = m_centerPt.Y* MAP_XY_FACTOR;
	pos.z = m_centerPt.Z* MAP_XY_FACTOR;

	Real groundLevel = getHeightAroundPos(m_heightMapRenderObj, pos.x, pos.y);

	// set position of camera itself
	/*
	sourcePos.X = m_cameraOffset.x;
	sourcePos.Y = m_cameraOffset.y;
	sourcePos.Z = m_cameraOffset.z;
	*/
	sourcePos.X = m_cameraOffset.x * zoom;
	sourcePos.Y = m_cameraOffset.y * zoom;
	sourcePos.Z = m_cameraOffset.z * zoom;

	// camera looking at origin
	targetPos.X = 0;
	targetPos.Y = 0;
	targetPos.Z = 0;


	Real factor = 1.0 - (groundLevel/sourcePos.Z );

	// construct a matrix to rotate around the up vector by the given angle
	Matrix3D angleTransform( Vector3( 0.0f, 0.0f, 1.0f ), angle );

	// construct a matrix to rotate around the horizontal vector by the given angle
	Matrix3D pitchTransform( Vector3( 1.0f, 0.0f, 0.0f ), pitch );

	// rotate camera position (pitch, then angle)
#ifdef ALLOW_TEMPORARIES
	sourcePos = pitchTransform * sourcePos;
	sourcePos = angleTransform * sourcePos;
#else
	pitchTransform.mulVector3(sourcePos);
	angleTransform.mulVector3(sourcePos);
#endif
	//sourcePos *= factor+zOffset;
	sourcePos *= factor;

	// translate to current XY position
	sourcePos.X += pos.x;
	sourcePos.Y += pos.y;
	sourcePos.Z += pos.z+groundLevel;

	targetPos.X += pos.x;
	targetPos.Y += pos.y;
	targetPos.Z += pos.z+groundLevel;

	// do fxPitch adjustment
	Real height = sourcePos.Z - targetPos.Z;
	height *= m_FXPitch;
	targetPos.Z = sourcePos.Z - height;

	// Just for kicks, lets see how high we are above the ground
	m_actualHeightAboveGround = m_cameraOffset.z * zoom - groundLevel;
	m_cameraSource = sourcePos;
	m_cameraTarget = targetPos;
	/*
	DEBUG_LOG(("Camera: pos=(%g,%g) height=%g pitch=%g FXPitch=%g yaw=%g groundLevel=%g",
		targetPos.X, targetPos.Y,
		m_actualHeightAboveGround,
		pitch,
		m_FXPitch,
		angle, m_groundLevel));
		*/

	// build new camera transform
	camtransform.Make_Identity();
	if (factor+zOffset < 0) {
		targetPos = sourcePos + (sourcePos-targetPos);
	}
	camtransform.Look_At( sourcePos, targetPos, 0 );
	/////////////////////////////////////////////////////////////
	targetPos.Z = 0;
	Real lookDistance = (targetPos-sourcePos).Length();
	Real nearZ, farZ;
	if (lookDistance < 300) lookDistance = 300;
	m_camera->Get_Clip_Planes(nearZ, farZ);
	m_camera->Set_Clip_Planes(lookDistance/200, lookDistance*3);

	if (m_projection) {
		camtransform.Make_Identity();
		camtransform.Set_Translation(Vector3(targetPos.X, targetPos.Y, lookDistance));
		m_heightMapRenderObj->setFlattenHeights(true);
		//m_camera->Set_Projection_Type(CameraClass::ORTHO);
	} else {
		m_heightMapRenderObj->setFlattenHeights(false);
		//m_camera->Set_Projection_Type(CameraClass::PERSPECTIVE);
	}
	m_camera->Set_Transform( camtransform );
	m_heightMapRenderObj->setDrawEntireMap(m_showEntireMap);
// not needed, handled in OnSize
//	m_camera->Set_Aspect_Ratio((float)m_actualWinSize.x/(float)m_actualWinSize.y);
}

// ----------------------------------------------------------------------------
void WbView3d::init3dScene()
{
	// build scene
	REF_PTR_RELEASE(m_overlayScene);
	REF_PTR_RELEASE(m_transparentObjectsScene);
	REF_PTR_RELEASE(m_baseBuildScene);
	REF_PTR_RELEASE(m_scene);
	REF_PTR_RELEASE(m_camera);
	REF_PTR_RELEASE(m_heightMapRenderObj);

	m_scene = NEW_REF(SkeletonSceneClass,());
#ifdef SAMPLE_DYNAMIC_LIGHT
	REF_PTR_RELEASE(theDynamicLight);
	theDynamicLight = NEW_REF(W3DDynamicLight, ());
	Real red = 1;
	Real green = 1;
	Real blue = 0;
	if(red==0 && blue==0 && green==0) {
		red = green = blue = 1;
	}
	theDynamicLight->Set_Ambient( Vector3( red, green, blue ) );
	theDynamicLight->Set_Diffuse( Vector3( red, green, blue) );
	theDynamicLight->Set_Position(Vector3(211, 363, 10));
	theDynamicLight->Set_Far_Attenuation_Range(5, 15);
	// Note: Don't Add_Render_Object dynamic lights.
	m_scene->addDynamicLight( theDynamicLight );
#endif
	m_overlayScene = NEW_REF(SkeletonSceneClass,());
	m_baseBuildScene = NEW_REF(SkeletonSceneClass,());
	m_transparentObjectsScene = NEW_REF(SkeletonSceneClass,());
//	m_scene->Set_Polygon_Mode(SceneClass::LINE);
	m_scene->Set_Ambient_Light(Vector3(0.5f,0.5f,0.5f));
	m_overlayScene->Set_Ambient_Light(Vector3(0.5f,0.5f,0.5f));
	m_baseBuildScene->Set_Ambient_Light(Vector3(0.5f,0.5f,0.5f));

	// Scene needs camera to be rendered with ----------------------------------
	m_camera = NEW_REF(CameraClass,());

}

// ----------------------------------------------------------------------------
void WbView3d::resetRenderObjects()
{
	if (!m_scene) return;
	if (TheW3DShadowManager) {
		TheW3DShadowManager->removeAllShadows();
	}

	SceneIterator *sceneIter = m_scene->Create_Iterator();
	sceneIter->First();
	while(!sceneIter->Is_Done()) {
		RenderObjClass * robj = sceneIter->Current_Item();
		robj->Add_Ref();
		m_scene->Remove_Render_Object(robj);
		robj->Release_Ref();
		sceneIter->Next();
	}
	m_scene->Destroy_Iterator(sceneIter);
	sceneIter = m_baseBuildScene->Create_Iterator();
	sceneIter->First();
	while(!sceneIter->Is_Done()) {
		RenderObjClass * robj = sceneIter->Current_Item();
		robj->Add_Ref();
		m_baseBuildScene->Remove_Render_Object(robj);
		robj->Release_Ref();
		sceneIter->Next();
	}
	m_baseBuildScene->Destroy_Iterator(sceneIter);
	MapObject *pMapObj = MapObject::getFirstMapObject();
	// Erase references to render objs that have been removed.
	while (pMapObj)
	{
		pMapObj->setRenderObj(nullptr);
		pMapObj = pMapObj->getNext();
	}

	Int i;
	for (i=0; i<TheSidesList->getNumSides(); i++) {
		SidesInfo *pSide = TheSidesList->getSideInfo(i);
		BuildListInfo *pBuild = pSide->getBuildList();
		while (pBuild) {
			pBuild->setRenderObj(nullptr);
			pBuild = pBuild->getNext();
		}
	}

	m_needToLoadRoads = true; // load roads next time we redraw.

	if (TheW3DShadowManager)
		TheW3DShadowManager->Reset();

	updateLights();
	if (m_heightMapRenderObj)
		m_scene->Add_Render_Object(m_heightMapRenderObj);
}

// ----------------------------------------------------------------------------
void WbView3d::stepTimeOfDay()
{
	TheWritableGlobalData->m_timeOfDay = (TimeOfDay)(TheGlobalData->m_timeOfDay+1);
	if (TheGlobalData->m_timeOfDay >= TIME_OF_DAY_COUNT) {
		TheWritableGlobalData->m_timeOfDay = TIME_OF_DAY_FIRST;
	}
	resetRenderObjects();
	invalObjectInView(nullptr);
}

// ----------------------------------------------------------------------------
void WbView3d::setLighting(const GlobalData::TerrainLighting *tl, Int whichLighting, Int whichLight)
{
	if (whichLighting == GlobalLightOptions::K_TERRAIN) {
		TheWritableGlobalData->m_terrainLighting[TheGlobalData->m_timeOfDay][whichLight]= *tl;
	} else if (whichLighting == GlobalLightOptions::K_OBJECTS) {
		TheWritableGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][whichLight]	= *tl;
	} else if (whichLighting == GlobalLightOptions::K_BOTH) {
		TheWritableGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][whichLight]	= *tl;
		TheWritableGlobalData->m_terrainLighting[TheGlobalData->m_timeOfDay][whichLight]	= *tl;
	}
	const GlobalData::TerrainLighting *ol = &TheGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][whichLight];
	TheWritableGlobalData->setTimeOfDay(TheGlobalData->m_timeOfDay);
	if( m_globalLight ) {
		m_globalLight[whichLight]->Set_Ambient( Vector3( 0.0f, 0.0f, 0.0f ) );
		m_globalLight[whichLight]->Set_Diffuse( Vector3(ol->diffuse.red, ol->diffuse.green, ol->diffuse.blue ) );
		m_globalLight[whichLight]->Set_Specular( Vector3(0,0,0) );
		Matrix3D mtx;
		mtx.Set(Vector3(1,0,0), Vector3(0,1,0), Vector3(ol->lightPos.x, ol->lightPos.y, ol->lightPos.z), Vector3(0,0,0));
		m_globalLight[whichLight]->Set_Transform(mtx);
		if( m_scene && whichLight == 0) {	//only let the first light contribute to ambient
			m_scene->Set_Ambient_Light( Vector3(ol->ambient.red, ol->ambient.green, ol->ambient.blue) );
			m_baseBuildScene->Set_Ambient_Light( Vector3(ol->ambient.red, ol->ambient.green, ol->ambient.blue) );
		}
	}
	if(TheTerrainRenderObject) {
		TheTerrainRenderObject->setTimeOfDay(TheGlobalData->m_timeOfDay);
	}
	if (TheW3DShadowManager) {
		TheW3DShadowManager->setTimeOfDay(TheGlobalData->m_timeOfDay);
	}
	m_needToLoadRoads = true; // load roads next time we redraw.
	Invalidate(false);
}

// ----------------------------------------------------------------------------
void WbView3d::updateLights()
{
	++m_updateCount;

	// Update lights list.
	m_lightList.Reset_List();

	{
		TheWritableGlobalData->setTimeOfDay(TheGlobalData->m_timeOfDay);
		const GlobalData::TerrainLighting *ol = &TheGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][0];

		if( m_scene )
		{
			m_scene->Set_Ambient_Light( Vector3(ol->ambient.red, ol->ambient.green, ol->ambient.blue) );
			m_baseBuildScene->Set_Ambient_Light( Vector3(ol->ambient.red, ol->ambient.green, ol->ambient.blue) );
		}

		if (TheW3DShadowManager) {
			TheW3DShadowManager->setTimeOfDay(TheGlobalData->m_timeOfDay);
		}

		for (Int i=0; i<MAX_GLOBAL_LIGHTS; i++)
		{

			if( m_globalLight[i] )
			{
				ol = &TheGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][i];
				m_globalLight[i]->Set_Ambient( Vector3( 0.0f, 0.0f, 0.0f ) );
				m_globalLight[i]->Set_Diffuse( Vector3(ol->diffuse.red, ol->diffuse.green, ol->diffuse.blue ) );
				m_globalLight[i]->Set_Specular( Vector3(0,0,0) );
				Matrix3D mtx;
				mtx.Set(Vector3(1,0,0), Vector3(0,1,0), Vector3(ol->lightPos.x, ol->lightPos.y, ol->lightPos.z), Vector3(0,0,0));
				m_globalLight[i]->Set_Transform(mtx);
 				m_scene->setGlobalLight(m_globalLight[i],i);
 				m_baseBuildScene->setGlobalLight(m_globalLight[i],i);
			}
		}
		if(TheTerrainRenderObject) {
			TheTerrainRenderObject->setTimeOfDay(TheGlobalData->m_timeOfDay);
		}


	}

	MapObject *pMapObj = MapObject::getFirstMapObject();
	while (pMapObj && m_heightMapRenderObj) {
		if (pMapObj->isLight()) {
			Coord3D loc = *pMapObj->getLocation();
			loc.z += m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, nullptr);
			RenderObjClass *renderObj= pMapObj->getRenderObj();
			if (renderObj) {
				m_scene->Remove_Render_Object(renderObj);
				pMapObj->setRenderObj(nullptr);
			}
			// It is a light, and handled at the device level.  jba.
			LightClass* lightP = NEW_REF(LightClass, (LightClass::POINT));

			Dict *props = pMapObj->getProperties();

			Real lightHeightAboveTerrain, lightInnerRadius, lightOuterRadius;
			RGBColor lightAmbientColor, lightDiffuseColor;
			lightHeightAboveTerrain = props->getReal(TheKey_lightHeightAboveTerrain);
			lightInnerRadius = props->getReal(TheKey_lightInnerRadius);
			lightOuterRadius = props->getReal(TheKey_lightOuterRadius);
			lightAmbientColor.setFromInt(props->getInt(TheKey_lightAmbientColor));
			lightDiffuseColor.setFromInt(props->getInt(TheKey_lightDiffuseColor));

			lightP->Set_Ambient( Vector3( lightAmbientColor.red, lightAmbientColor.green, lightAmbientColor.blue ) );
			lightP->Set_Diffuse( Vector3(  lightDiffuseColor.red, lightDiffuseColor.green, lightDiffuseColor.blue) );

			lightP->Set_Position(Vector3(loc.x, loc.y, loc.z+lightHeightAboveTerrain));

			lightP->Set_Far_Attenuation_Range(lightInnerRadius, lightOuterRadius);

			m_lightList.Add(lightP);
 			m_scene->Add_Render_Object(lightP);
			pMapObj->setRenderObj(lightP);
			REF_PTR_RELEASE( lightP );
		}
		pMapObj = pMapObj->getNext();
	}

	--m_updateCount;
}

// ----------------------------------------------------------------------------
void WbView3d::updateScorches()
{
	TheTerrainRenderObject->clearAllScorches();
	MapObject *pMapObj;
	for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext())
	{
		if (pMapObj->isScorch())
		{
			const Coord3D *pos = pMapObj->getLocation();
			Real radius = pMapObj->getProperties()->getReal(TheKey_objectRadius);
			Scorches type = (Scorches) pMapObj->getProperties()->getInt(TheKey_scorchType);

			Vector3 loc(pos->x, pos->y, pos->z);
			TheTerrainRenderObject->addScorch(loc, radius, type);
		}
	}
}

// ----------------------------------------------------------------------------
void WbView3d::invalidateCellInView(int xIndex, int yIndex)
{
	Invalidate(false);	/// @todo be smarter about invaling the area
}

// ----------------------------------------------------------------------------
void WbView3d::updateFenceListObjects(MapObject *pObject)
{
	MapObject *pMapObj;
	for (pMapObj = pObject; pMapObj; pMapObj = pMapObj->getNext())
	{

		Coord3D loc = *pMapObj->getLocation();
		loc.z += m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, nullptr);

		RenderObjClass *renderObj=nullptr;
		REF_PTR_SET( renderObj, pMapObj->getRenderObj() );
		if (!renderObj) {
			Real scale = 1.0;
			AsciiString modelName = getModelNameAndScale(pMapObj, &scale, BODY_PRISTINE);
			// set render object, or create if we need to
			if( renderObj == nullptr && modelName.isEmpty() == FALSE &&
					strncmp( modelName.str(), "No ", 3 ) != 0 )
			{

				renderObj = m_assetManager->Create_Render_Obj( modelName.str(), scale, 0);

			}
		}

		if (renderObj) {
			pMapObj->setRenderObj(renderObj);

			// set item's position to loc, and get scale from item and apply it.

			Matrix3D renderObjPos(true);	// init to identity
			renderObjPos.Translate(loc.x, loc.y, loc.z);
			renderObjPos.Rotate_Z(pMapObj->getAngle());
			renderObj->Set_Transform( renderObjPos );


			m_scene->Add_Render_Object(renderObj);

			REF_PTR_RELEASE(renderObj); // belongs to m_scene now.
		}
	}

	Invalidate(false);
}


// ----------------------------------------------------------------------------
void WbView3d::removeFenceListObjects(MapObject *pObject)
{
	MapObject *pMapObj;
	for (pMapObj = pObject; pMapObj; pMapObj = pMapObj->getNext())
	{
		if (pMapObj->getRenderObj()) {
			m_scene->Remove_Render_Object(pMapObj->getRenderObj());
			pMapObj->setRenderObj(nullptr);
		}
	}

	Invalidate(false);
}

// ----------------------------------------------------------------------------
/// @todo srj -- this is a terrible hack, since things can have multiple models, and it's private info. fix later.
AsciiString WbView3d::getBestModelName(const ThingTemplate* tt, const ModelConditionFlags& c)
{
	if (tt)
	{
		const ModuleInfo& mi = tt->getDrawModuleInfo();
		if (mi.getCount() > 0)
		{
//		const W3DModelDrawModuleData* md = dynamic_cast<const W3DModelDrawModuleData*>(mi->getNthData(0));
			const ModuleData* mdd = mi.getNthData(0);
			const W3DModelDrawModuleData* md = mdd ? mdd->getAsW3DModelDrawModuleData() : nullptr;
			if (md)
			{
				return md->getBestModelNameForWB(c);
			}
		}
	}
	// removing this crash as sounds can (and should) have no model - jkmcd
	return AsciiString::TheEmptyString;
}

// ----------------------------------------------------------------------------
void WbView3d::invalBuildListItemInView(BuildListInfo *pBuildToInval)
{
	Int i;
	Bool found = false;

	for (i=0; i<TheSidesList->getNumSides(); i++) {
		SidesInfo *pSide = TheSidesList->getSideInfo(i);

		// find which player color we should use
		Int playerColor = 0xFFFFFF;
		const Dict *sideDict = pSide->getDict();
		if (sideDict)
		{
			Bool exists = false;
			Int color = sideDict->getInt(TheKey_playerColor, &exists);
			if (exists)
			{
				playerColor = color;
			}
		}


		for (BuildListInfo *pBuild = pSide->getBuildList(); pBuild; pBuild = pBuild->getNext()) {
			if (pBuildToInval == pBuild) {
				found = true;
			}
			if (!found && pBuildToInval) {
				continue;
			}
			if (!BuildListTool::isActive() && !pBuild->isInitiallyBuilt()) {
				continue;
			}
			// Update.
			Coord3D loc = *pBuild->getLocation();
			loc.z += m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, nullptr);
			RenderObjClass *renderObj=nullptr;
			Shadow			*shadowObj=nullptr;
			// Build list render obj is not refcounted, so check & make sure it exists in the scene.
			if (pBuild->getRenderObj()) {
				if (!m_baseBuildScene->safeContains(pBuild->getRenderObj())) {
					pBuild->setRenderObj(nullptr);
				}
			}

			REF_PTR_SET(renderObj, pBuild->getRenderObj());
			if (!renderObj) {
				Real scale = 1.0;
				AsciiString thingName = pBuild->getTemplateName();
				const ThingTemplate *tTemplate = TheThingFactory->findTemplate(thingName);

				AsciiString modelName = "No Model Name";
				if (tTemplate) {
					ModelConditionFlags state;
					state.clear();
					modelName = getBestModelName(tTemplate, state);
					scale = tTemplate->getAssetScale();
				}
				// set render object, or create if we need to
				if( renderObj == nullptr && modelName.isEmpty() == FALSE &&
						strncmp( modelName.str(), "No ", 3 ) != 0 )
				{

					renderObj = m_assetManager->Create_Render_Obj( modelName.str(), scale, playerColor);
					if( m_showShadows  && tTemplate->getShadowType() != SHADOW_NONE)
					{
						//add correct type of shadow
						Shadow::ShadowTypeInfo shadowInfo;
						shadowInfo.allowUpdates=FALSE;	//shadow image will never update
						shadowInfo.allowWorldAlign=TRUE;	//shadow image will wrap around world objects
						strlcpy(shadowInfo.m_ShadowName, tTemplate->getShadowTextureName().str(), ARRAY_SIZE(shadowInfo.m_ShadowName));
						DEBUG_ASSERTCRASH(shadowInfo.m_ShadowName[0] != '\0', ("this should be validated in ThingTemplate now"));
						shadowInfo.m_type=(ShadowType)tTemplate->getShadowType();
						shadowInfo.m_sizeX=tTemplate->getShadowSizeX();
						shadowInfo.m_sizeY=tTemplate->getShadowSizeY();
						shadowInfo.m_offsetX=tTemplate->getShadowOffsetX();
						shadowInfo.m_offsetY=tTemplate->getShadowOffsetY();
						shadowObj=TheW3DShadowManager->addShadow(renderObj, &shadowInfo);
					}
				}
			}
			if (renderObj) {
				pBuild->setRenderObj(renderObj);
				pBuild->setShadowObj(shadowObj);
				// set item's position to loc.
				Matrix3D renderObjPos(true);	// init to identity
				renderObjPos.Translate(loc.x, loc.y, loc.z);
				renderObjPos.Rotate_Z(pBuild->getAngle());
				renderObj->Set_Transform( renderObjPos );

				m_baseBuildScene->Add_Render_Object(renderObj);

				REF_PTR_RELEASE(renderObj); // belongs to m_scene now.
			}
			if (found) break;
		}
	}

	// Build list render obj is not refcounted, so check & make sure it exists in the scene.
	if (!found && pBuildToInval && pBuildToInval->getRenderObj()) {
		if (!m_baseBuildScene->safeContains(pBuildToInval->getRenderObj())) {
			pBuildToInval->setRenderObj(nullptr);
		}
	}
	if (!found && pBuildToInval && pBuildToInval->getRenderObj()) {
		m_baseBuildScene->Remove_Render_Object(pBuildToInval->getRenderObj());
		pBuildToInval->setRenderObj(nullptr);
	}
	Invalidate(false);
}


AsciiString WbView3d::getModelNameAndScale(MapObject *pMapObj, Real *scale, BodyDamageType curDamageState)
{
	ModelConditionFlags state;
	switch (curDamageState)
	{
			case BODY_PRISTINE:
			default:
				state.clear();
				break;

			case BODY_DAMAGED:
				state.set(MODELCONDITION_DAMAGED);
				break;

			case BODY_REALLYDAMAGED:
				state.set(MODELCONDITION_REALLY_DAMAGED);
				break;

			case BODY_RUBBLE:
				state.set(MODELCONDITION_RUBBLE);
				break;
	}

	if (getShowGarrisoned())
	{
		state.set(MODELCONDITION_GARRISONED);
	}
	Int objWeather = 0;
	Bool exists;
	if (pMapObj && pMapObj->getProperties())
	{
		objWeather = pMapObj->getProperties()->getInt(TheKey_objectWeather, &exists);
	}
	switch (objWeather)
	{
		default:
		case 0:
			if (TheGlobalData->m_weather == WEATHER_SNOWY)
			{
				state.set(MODELCONDITION_SNOW);
			}
			break;
		case 2:
			state.set(MODELCONDITION_SNOW);
			break;
	}

	Int objTime = 0;
	if (pMapObj && pMapObj->getProperties())
	{
		objTime = pMapObj->getProperties()->getInt(TheKey_objectTime, &exists);
	}
	switch (objTime)
	{
		default:
		case 0:
			if (TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT)
			{
				state.set(MODELCONDITION_NIGHT);
			}
			break;
		case 2:
			state.set(MODELCONDITION_NIGHT);
			break;
	}

	AsciiString modelName;
	*scale = 1.0f;

#ifdef LOAD_TEST_ASSETS
	char buffer[ _MAX_PATH ];

	if (strncmp(TEST_STRING, pMapObj->getName().str(), strlen(TEST_STRING)) == 0)
	{
		/* Handle test art models here */
		strlcpy(buffer, pMapObj->getName().str(), ARRAY_SIZE(buffer));

		Int i;
		for (i=0; buffer[i]; i++) {
			if (buffer[i] == '/') {
				i++;
				break;
			}
		}
		modelName = buffer+i;
	}
#endif

	if (modelName.isEmpty())
	{
		modelName = "No Model Name"; // must be this while GDF exists (it's the default)
		const ThingTemplate *tTemplate;

		tTemplate = pMapObj->getThingTemplate();
		if( tTemplate && !(pMapObj->getFlags() & FLAG_DONT_RENDER))
		{

			// get visual data from the thing template
			modelName = getBestModelName(tTemplate, state);
			*scale = tTemplate->getAssetScale();

		}
	}
	return modelName;
}

// ----------------------------------------------------------------------------
void WbView3d::invalObjectInView(MapObject *pMapObjIn)
{
	++m_updateCount;
	if (m_heightMapRenderObj == nullptr) {
		m_heightMapRenderObj = NEW_REF(WBHeightMap,());

		m_scene->Add_Render_Object(m_heightMapRenderObj);
	}
	if (pMapObjIn == nullptr) {
		invalBuildListItemInView(nullptr);
	}
	Bool found = false;
	Bool isRoad = false;
	Bool isLight = false;
	Bool isScorch = false;
	if (pMapObjIn == nullptr)
		isScorch = true;
	MapObject *pMapObj;
	for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext())
	{
		if (pMapObjIn == pMapObj)
			found = true;
		if (pMapObjIn != nullptr && !found) {
			continue;
		}
		if (pMapObj->getFlags() & (FLAG_ROAD_FLAGS|FLAG_BRIDGE_FLAGS)) {
			isRoad = true;
			continue; // Roads don't create drawable objects.
		}
		if (pMapObj->isLight() ) {
			isLight = true;
			continue; // Lights don't create drawable objects.
		}
		if (pMapObj->isScorch()) {
			if (pMapObj == pMapObjIn) {
				isScorch = true;
			}
			continue;
		}


		Coord3D loc = *pMapObj->getLocation();
		loc.z += m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, nullptr);

		RenderObjClass *renderObj=nullptr;
		Shadow		   *shadowObj=nullptr;

		REF_PTR_SET( renderObj, pMapObj->getRenderObj() );
		Int playerColor = 0xFFFFFF;
		BodyDamageType curDamageState = BODY_PRISTINE;
		Bool isVehicle = false;
		if (pMapObj)
		{
			Bool exists;
			const ThingTemplate* tt = pMapObj->getThingTemplate();
			if (tt && !(pMapObj->getFlags() & FLAG_DONT_RENDER))
			{
				isVehicle = tt->isKindOf(KINDOF_VEHICLE);
				AsciiString objectTeamName = pMapObj->getProperties()->getAsciiString(TheKey_originalOwner, &exists);
				if (exists) {
					TeamsInfo *teamInfo = TheSidesList->findTeamInfo(objectTeamName);
					if (teamInfo) {
						AsciiString teamOwner = teamInfo->getDict()->getAsciiString(TheKey_teamOwner);
						SidesInfo* pSide = TheSidesList->findSideInfo(teamOwner);
						if (pSide) {
							Bool hasColor = false;
							Int color = pSide->getDict()->getInt(TheKey_playerColor, &hasColor);
							if (hasColor) {
								playerColor = color;
							} else {
								AsciiString tmplname = pSide->getDict()->getAsciiString(TheKey_playerFaction);
								const PlayerTemplate* pt = ThePlayerTemplateStore->findPlayerTemplate(NAMEKEY(tmplname));
								if (pt) {
									playerColor = pt->getPreferredColor()->getAsInt();
								}
							}
						}
					}
				}
			}
			Int health = 100;
			health = pMapObj->getProperties()->getInt(TheKey_objectInitialHealth, &exists);
			Real ratio = health/100.0;
			if (ratio > TheGlobalData->m_unitDamagedThresh)
			{
				curDamageState = BODY_PRISTINE;
			}
			else if (ratio > TheGlobalData->m_unitReallyDamagedThresh)
			{
				curDamageState = BODY_DAMAGED;
			}
			else if (ratio > 0.0f)
			{
				curDamageState = BODY_REALLYDAMAGED;
			}
			else
			{
				curDamageState = BODY_RUBBLE;
			}

		}
		if (!renderObj) {
			Real scale = 1.0;
			AsciiString modelName = getModelNameAndScale(pMapObj, &scale, curDamageState);
			// set render object, or create if we need to
			if( renderObj == nullptr && modelName.isEmpty() == FALSE &&
					strncmp( modelName.str(), "No ", 3 ) != 0 )
			{

				if (!getShowModels()) {
					continue;
				}
				renderObj = m_assetManager->Create_Render_Obj( modelName.str(), scale, playerColor);
				if( m_showShadows )
				{
					Shadow::ShadowTypeInfo shadowInfo;
					shadowInfo.allowUpdates=FALSE;	//shadow image will never update
					shadowInfo.allowWorldAlign=TRUE;	//shadow image will wrap around world objects
					const ThingTemplate *tTemplate = pMapObj->getThingTemplate();
					if (tTemplate && tTemplate->getShadowType() != SHADOW_NONE && !(pMapObj->getFlags() & FLAG_DONT_RENDER))
					{	//add correct type of shadow
						strlcpy(shadowInfo.m_ShadowName, tTemplate->getShadowTextureName().str(), ARRAY_SIZE(shadowInfo.m_ShadowName));
						DEBUG_ASSERTCRASH(shadowInfo.m_ShadowName[0] != '\0', ("this should be validated in ThingTemplate now"));
						shadowInfo.m_type=(ShadowType)tTemplate->getShadowType();
						shadowInfo.m_sizeX=tTemplate->getShadowSizeX();
						shadowInfo.m_sizeY=tTemplate->getShadowSizeY();
						shadowInfo.m_offsetX=tTemplate->getShadowOffsetX();
						shadowInfo.m_offsetY=tTemplate->getShadowOffsetY();
						shadowObj=TheW3DShadowManager->addShadow(renderObj, &shadowInfo);
					} else if (!tTemplate) {
						shadowInfo.m_type=(ShadowType)SHADOW_VOLUME;
						shadowObj=TheW3DShadowManager->addShadow(renderObj, &shadowInfo);
					}
				}
			}
		}

		if (renderObj && !(pMapObj->getFlags() & FLAG_DONT_RENDER)) {
			pMapObj->setRenderObj(renderObj);
			pMapObj->setShadowObj(shadowObj);

			// set item's position to loc, and get scale from item and apply it.

			Matrix3D renderObjPos(true);	// init to identity
			renderObjPos.Translate(loc.x, loc.y, loc.z);
			renderObjPos.Rotate_Z(pMapObj->getAngle());
			renderObj->Set_Transform( renderObjPos );

			if (isVehicle) {
				// note that this affects our orientation, but NOT our position... specifically,
				// it does NOT force us to "stick" to the ground!
				Matrix3D mtx;
				Coord3D terrainNormal;
				m_heightMapRenderObj->getHeightMapHeight(loc.x, loc.y, &terrainNormal );
				makeAlignToNormalMatrix(pMapObj->getAngle(), loc, terrainNormal, mtx);
				renderObj->Set_Transform( mtx );
			}

			m_scene->Add_Render_Object(renderObj);

			REF_PTR_RELEASE(renderObj); // belongs to m_scene now.
		} else if (renderObj) {
			m_scene->Remove_Render_Object(renderObj);
		}
		if (found) break;
	}
	if (!found && pMapObjIn) {
		if (pMapObjIn->getFlags() & (FLAG_ROAD_FLAGS|FLAG_BRIDGE_FLAGS)) {
			isRoad = true;
		}
	}
	if (!found && pMapObjIn && pMapObjIn->getRenderObj()) {
		if( m_showShadows ) {
			resetRenderObjects();
			invalObjectInView(nullptr);
			--m_updateCount;
			return;
		}
		m_scene->Remove_Render_Object(pMapObjIn->getRenderObj());
		pMapObjIn->setRenderObj(nullptr);
	}

	if (isRoad) {
		m_needToLoadRoads = true; // load roads next time we redraw.
	}
	if (isLight) {
		updateLights();
	}
	if (isScorch) {
		updateScorches();
	}
	Invalidate(false);

	--m_updateCount;
}


// ----------------------------------------------------------------------------
void WbView3d::updateHeightMapInView(WorldHeightMap *htMap, Bool partial, const IRegion2D &partialRange)
{
	if (htMap == nullptr)
		return;

	++m_updateCount;

	if (m_heightMapRenderObj == nullptr) {
		m_heightMapRenderObj = NEW_REF(WBHeightMap,());
		m_scene->Add_Render_Object(m_heightMapRenderObj);
		partial = false;
	}


	if (m_heightMapRenderObj) {

		Int curTicks = ::GetTickCount();

		RefRenderObjListIterator lightListIt(&m_lightList);
		if (partial) {
			m_heightMapRenderObj->doPartialUpdate(partialRange, htMap, &lightListIt);
		} else {
			if (m_showEntireMap) {
				htMap->setDrawOrg(0, 0);
				htMap->setDrawWidth(htMap->getXExtent());
				htMap->setDrawHeight(htMap->getYExtent());
				m_heightMapRenderObj->initHeightData(htMap->getXExtent(), htMap->getYExtent(), htMap, &lightListIt);
			} else {
				htMap->setDrawWidth(std::min(m_partialMapSize, htMap->getXExtent()));
				htMap->setDrawHeight(std::min(m_partialMapSize, htMap->getYExtent()));
				m_heightMapRenderObj->initHeightData(htMap->getDrawWidth(), htMap->getDrawHeight(), htMap, &lightListIt);
			}
			m_heightMapRenderObj->updateViewImpassableAreas();
		}
		curTicks = GetTickCount() - curTicks;
		if (curTicks < 1) curTicks = 1;
	}

	invalObjectInView(nullptr);	// update all the map objects, to account for ground changes

	--m_updateCount;
}

// ----------------------------------------------------------------------------
void WbView3d::setCenterInView(Real x, Real y)
{
	if (x != m_centerPt.X || y != m_centerPt.Y) {
		m_centerPt.X = x;
		m_centerPt.Y = y;
		constrainCenterPt();
		redraw();
		updateHysteresis();
		drawLabels();
		CMainFrame::GetMainFrame()->handleCameraChange();
	}
}

//=============================================================================
// WbView3d::picked3dObjectInView
//=============================================================================
/** Returns true if the pixel location picks the object. */
//=============================================================================
MapObject *WbView3d::picked3dObjectInView(CPoint viewPt)
{
	// This code picks on all 3d objects.
	if (m_intersector && m_layer) {
		CRect client;
		this->GetClientRect(&client);
		float logX = (Real)viewPt.x / (Real)client.Width();
		float logY = (Real)viewPt.y / (Real)client.Height();
		//m_intersector->Result.CollisionType = COLLISION_TYPE_0|COLLISION_TYPE_1;
		// do the intersection using W3D intersector class
		Bool hit = m_intersector->Intersect_Screen_Point_Layer( logX, logY, *m_layer );
		if( hit )
		{
			MapObject *pObj;
			for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
				if (pObj->getRenderObj() == m_intersector->Result.IntersectedRenderObject) {
					return pObj;
				}
			}
		}
	}

	return nullptr;
}

//=============================================================================
// WbView3d::pickedBuildObjectInView
//=============================================================================
/** Returns true if the pixel location picks the object. */
//=============================================================================
BuildListInfo *WbView3d::pickedBuildObjectInView(CPoint viewPt)
{
	Coord3D cpt;
	Int i;
	viewToDocCoords(viewPt, &cpt, false);
 	for (i=0; i<TheSidesList->getNumSides(); i++) {
		SidesInfo *pSide = TheSidesList->getSideInfo(i);
		for (BuildListInfo *pBuild = pSide->getBuildList(); pBuild; pBuild = pBuild->getNext()) {
			Coord3D center = *pBuild->getLocation();
			center.x -= cpt.x;
			center.y -= cpt.y;
			center.z = 0;
			Real len = center.length();
			// Check and see if we are within 1.5 cell size of the center.
			if (len < 1.5f*MAP_XY_FACTOR) {
				return pBuild;
			}
		}
	}
	// This code picks on all 3d build objects.
	if (m_intersector && m_buildLayer) {
		CRect client;
		this->GetClientRect(&client);
		float logX = (Real)viewPt.x / (Real)client.Width();
		float logY = (Real)viewPt.y / (Real)client.Height();

		// do the intersection using W3D intersector class
		Bool hit = m_intersector->Intersect_Screen_Point_Layer( logX, logY, *m_buildLayer );
		if( hit ) {
 			for (i=0; i<TheSidesList->getNumSides(); i++) {
				SidesInfo *pSide = TheSidesList->getSideInfo(i);
				for (BuildListInfo *pBuild = pSide->getBuildList(); pBuild; pBuild = pBuild->getNext()) {
					if (pBuild->getRenderObj() == m_intersector->Result.IntersectedRenderObject) {
						return pBuild;
					}
				}
			}
		}
	}

	return nullptr;
}

// ----------------------------------------------------------------------------
Bool WbView3d::viewToDocCoords(CPoint curPt, Coord3D *newPt, Bool constrain)
{
	DEBUG_ASSERTCRASH((this),("oops"));
//	const Int VIEW_BORDER = 3000;  // keeps you from falling off the edge of the world.
	Bool result = false;
	CRect client;
	this->GetClientRect(&client);

	// get our "logical" or relative screen coords
	float logX = (Real)curPt.x / (Real)client.Width();
	float logY = (Real)curPt.y / (Real)client.Height();
	Vector3 intersection(0,0,0);
	// determine the ray corresponding to the camera and distance to projection plane
	Matrix3D camera_matrix = m_camera->Get_Transform();

	Vector3 camera_location  = m_camera->Get_Position();

	Vector3 rayLocation;
	Vector3 rayDirection;
	Vector3 rayDirectionPt;
	// the projected ray has the same origin as the camera
	rayLocation = camera_location;
	// determine the location of the screen coordinate in camera-model space
	const ViewportClass &viewport = m_camera->Get_Viewport();

	Vector2 min,max;
	m_camera->Get_View_Plane(min,max);
	float xscale = (max.X - min.X);
	float yscale = (max.Y - min.Y);

	float zmod = -1.0; // Scene->vpd; // Note: view plane distance is now always 1.0 from the camera
	float xmod = (-logX + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
	float ymod = (logY - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

	// transform the screen coordinates by the camera's matrix into world coordinates.
	float x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
	float y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
	float z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];

	rayDirection.Set(x,y,z);
	rayDirection.Normalize();
	float MaxDistance =  m_camera->Get_Depth()*MAP_XY_FACTOR;
	rayDirectionPt = rayLocation + rayDirection*MaxDistance;

	LineSegClass ray(rayLocation, rayDirectionPt);

	// Note - there are 2 ways to track.  One is for tools (like paint texture)
	// that follow the terrain.  They want to track the terrain, so the texturing
	// follows the cursor.  Most tools, however, don't want to jump up & down clifs
	// and such.  So they use a fixed z plane when tracking, so things don't move
	// depending what you move over.
	Bool followTerrain = true;
	if (WbApp()->isCurToolLocked()) {
		followTerrain = WbApp()->getCurTool()->followsTerrain();
	}
	if (followTerrain) {
		CastResultStruct castResult;
		RayCollisionTestClass rayCollide(ray, &castResult) ;
		if( TheTerrainRenderObject->Cast_Ray(rayCollide) )
		{
			// get the point of intersection according to W3D
			intersection = castResult.ContactPoint;
			m_curTrackingZ = intersection.Z;
			result = true;
		}
	}
	if (!result) {
		intersection.X = Vector3::Find_X_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
		intersection.Y = Vector3::Find_Y_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
		result = true;
	}
	newPt->x = intersection.X;
	newPt->y = intersection.Y;
	newPt->z = MAGIC_GROUND_Z;
	if (constrain) {
#if 1
		if (m_doLockAngle) {
			Real dy = fabs(m_mouseDownDocPoint.y - newPt->y);
			Real dx = fabs(m_mouseDownDocPoint.x - newPt->x);
			if (dx>2*dy) {
				// lock to dx.
				newPt->y = m_mouseDownDocPoint.y;
			} else if (dy>2*dx) {
				//lock to dy.
				newPt->x = m_mouseDownDocPoint.x;
			} else {
				// Lock to 45 degree.
				dx = (dx+dy)/2;
				dy = dx;
				if (newPt->x < m_mouseDownDocPoint.x) dx = -dx;
				if (newPt->y < m_mouseDownDocPoint.y) dy = -dy;
				newPt->x = m_mouseDownDocPoint.x+dx;
				newPt->y = m_mouseDownDocPoint.y+dy;
			}
		}
#else
		if (m_cameraAngle > PI) {
			m_cameraAngle -= 2*PI;
		}
		if (m_cameraAngle < -PI) {
			m_cameraAngle += 2*PI;
		}
		Bool flip = false;
		// If we are looking sideways, flip the locks.
		if (PI/4<m_cameraAngle && m_cameraAngle < 3*PI/4) {
			flip = true;
		}
		if (-PI/4>m_cameraAngle && m_cameraAngle > -3*PI/4) {
			flip = true;
		}

		if (flip) {
			if (m_doLockVertical) {
				newPt->y = m_mouseDownDocPoint.y;
			} else if (m_doLockHorizontal) {
				newPt->x = m_mouseDownDocPoint.x;
			}
		} else {
			if (m_doLockHorizontal) {
				newPt->y = m_mouseDownDocPoint.y;
			} else if (m_doLockVertical) {
				newPt->x = m_mouseDownDocPoint.x;
			}
		}
#endif
	}
	return result;
}

// ----------------------------------------------------------------------------
Bool WbView3d::viewToDocCoordZ(CPoint curPt, Coord3D *newPt, Real theZ)
{
	DEBUG_ASSERTCRASH((this),("oops"));
	CRect client;
	this->GetClientRect(&client);

	// get our "logical" or relative screen coords
	float logX = (Real)curPt.x / (Real)client.Width();
	float logY = (Real)curPt.y / (Real)client.Height();
	Vector3 intersection(0,0,0);
	// determine the ray corresponding to the camera and distance to projection plane
	Matrix3D camera_matrix = m_camera->Get_Transform();

	Vector3 camera_location  = m_camera->Get_Position();

	Vector3 rayLocation;
	Vector3 rayDirection;
	Vector3 rayDirectionPt;
	// the projected ray has the same origin as the camera
	rayLocation = camera_location;
	// determine the location of the screen coordinate in camera-model space
	const ViewportClass &viewport = m_camera->Get_Viewport();

	Vector2 min,max;
	m_camera->Get_View_Plane(min,max);
	float xscale = (max.X - min.X);
	float yscale = (max.Y - min.Y);

	float zmod = -1.0; // Scene->vpd; // Note: view plane distance is now always 1.0 from the camera
	float xmod = (-logX + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
	float ymod = (logY - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

	// transform the screen coordinates by the camera's matrix into world coordinates.
	float x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
	float y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
	float z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];

	rayDirection.Set(x,y,z);
	rayDirection.Normalize();
	float MaxDistance =  m_camera->Get_Depth()*MAP_XY_FACTOR;
	rayDirectionPt = rayLocation + rayDirection*MaxDistance;

	LineSegClass ray(rayLocation, rayDirectionPt);

	intersection.X = Vector3::Find_X_At_Z(theZ, rayLocation, rayDirectionPt);
	intersection.Y = Vector3::Find_Y_At_Z(theZ, rayLocation, rayDirectionPt);

	newPt->x = intersection.X;
	newPt->y = intersection.Y;
	newPt->z = theZ;
	return true;
}

// ----------------------------------------------------------------------------
void WbView3d::updateHysteresis()
{
	CRect client;
	GetClientRect(&client);
	CPoint curPt;
	curPt.x = (client.left+client.right)/2;
	curPt.y = (client.bottom+client.top)/2;
	// get our "logical" or relative screen coords
	float logX = (Real)curPt.x / (Real)client.Width();
	float logY = (Real)curPt.y / (Real)client.Height();
	Vector3 intersection(0,0,0);
	// determine the ray corresponding to the camera and distance to projection plane
	Matrix3D camera_matrix = m_camera->Get_Transform();

	Vector3 camera_location  = m_camera->Get_Position();

	Vector3 rayLocation;
	Vector3 rayDirection;
	Vector3 rayDirectionPt;
	// the projected ray has the same origin as the camera
	rayLocation = camera_location;
	// determine the location of the screen coordinate in camera-model space
	const ViewportClass &viewport = m_camera->Get_Viewport();

	Vector2 min,max;
	m_camera->Get_View_Plane(min,max);
	float xscale = (max.X - min.X);
	float yscale = (max.Y - min.Y);

	float zmod = -1.0; // Scene->vpd; // Note: view plane distance is now always 1.0 from the camera
	float xmod = (-logX + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
	float ymod = (logY - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

	// transform the screen coordinates by the camera's matrix into world coordinates.
	float x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
	float y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
	float z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];

	rayDirection.Set(x,y,z);
	rayDirectionPt = rayLocation + rayDirection;

	intersection.X = Vector3::Find_X_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
	intersection.Y = Vector3::Find_Y_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);

	// Calculate the point offset by 3 pixels.
	logX = (Real)(curPt.x+3) / (Real)client.Width();
	Vector3 offset(0,0,0);

	xmod = (-logX + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
	ymod = (logY - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

	// transform the screen coordinates by the camera's matrix into world coordinates.
	x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
	y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
	z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];
	rayDirection.Set(x,y,z);
	rayDirectionPt = rayLocation + rayDirection;
	offset.X = Vector3::Find_X_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
	offset.Y = Vector3::Find_Y_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
	offset = offset - intersection;
	m_hysteresis = offset.Length();

	logX = (Real)(curPt.x) / (Real)client.Width();
	logY = (Real)(curPt.y+3) / (Real)client.Height();

	xmod = (-logX + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
	ymod = (logY - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

	// transform the screen coordinates by the camera's matrix into world coordinates.
	x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
	y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
	z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];
	rayDirection.Set(x,y,z);
	rayDirectionPt = rayLocation + rayDirection;
	offset.X = Vector3::Find_X_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
	offset.Y = Vector3::Find_Y_At_Z(m_curTrackingZ, rayLocation, rayDirectionPt);
	offset = offset - intersection;
	if (m_hysteresis < offset.Length()) m_hysteresis = offset.Length();

	CPoint pt1, pt2;
	Coord3D loc;
	loc.x = intersection.X;
	loc.y = intersection.Y;
	loc.z = intersection.Z;
	this->docToViewCoords(loc, &pt1);
	loc.x += MAP_XY_FACTOR*0.4f;
	loc.y += MAP_XY_FACTOR*0.4f;
	this->docToViewCoords(loc, &pt2);
	Int dx = pt1.x-pt2.x;
	if (dx<0) dx = -dx;
	Int dy = pt1.y-pt2.y;
	if (dy<0) dy = -dy;
	if (dx<dy) dx = dy;
	if (dx<4) dx = 3;
	m_pickPixels = dx+3;

}

// ----------------------------------------------------------------------------
Bool WbView3d::docToViewCoords(Coord3D curPt, CPoint* newPt)
{
	Vector3 world;
	Vector3 screen;
	newPt->x = -1000;
	newPt->y = -1000;
	if (m_heightMapRenderObj) {
		curPt.z += m_heightMapRenderObj->getHeightMapHeight(curPt.x, curPt.y, nullptr);
	}

	world.Set( curPt.x, curPt.y, curPt.z );
	if (m_camera->Project( screen, world ) != CameraClass::INSIDE_FRUSTUM) return false;

	CRect rClient;
	GetClientRect(&rClient);

	//
	// note that the screen coord returned from the project W3D camera
	// gave us a screen coords that range from (-1,-1) bottom left to
	// (1,1) top right ... we are turning that into (0,0) upper left
	// coords now
	//
	Int sx, sy;
	W3DLogicalScreenToPixelScreen( screen.X, screen.Y,
																 &sx, &sy,
																 rClient.right-rClient.left, rClient.bottom-rClient.top );

	newPt->x = rClient.left + sx;
	newPt->y = rClient.top + sy;

	return true;
}

// ----------------------------------------------------------------------------
void WbView3d::redraw()
{
	if (m_updateCount > 0) {
		return;
	}
	if (IsIconic()) {
		return;
	}
	if (!IsWindowVisible()) {
		return;
	}
	if (!m_ww3dInited) {
		return;
	}

	setupCamera();

	DEBUG_ASSERTCRASH((m_heightMapRenderObj),("oops"));
	if (m_heightMapRenderObj) {
		if (m_needToLoadRoads) {
			m_heightMapRenderObj->loadRoadsAndBridges(nullptr,FALSE);
			m_heightMapRenderObj->worldBuilderUpdateBridgeTowers( m_assetManager, m_scene );
			m_needToLoadRoads = false;
		}
		++m_updateCount;
		Int curTicks = GetTickCount();
		RefRenderObjListIterator lightListIt(&m_lightList);
		m_heightMapRenderObj->updateCenter(m_camera, &m_cameraTarget, &lightListIt);
		--m_updateCount;

		curTicks = GetTickCount()-curTicks;
//		if (curTicks>2) {
//			WWDEBUG_SAY(("%d ms for updateCenter, %d FPS", curTicks, 1000/curTicks));
//		}
	}
	if (m_drawObject) {
		m_drawObject->setDrawObjects(m_showObjects,
			m_showWaypoints || WaypointTool::isActive(),
			m_showPolygonTriggers || PolygonTool::isActive());
	}

	WW3D::Update_Logic_Frame_Time(TheFramePacer->getLogicTimeStepMilliseconds());
	WW3D::Sync(WW3D::Get_Fractional_Sync_Milliseconds() >= WWSyncMilliseconds);

	m_buildRedMultiplier += (GetTickCount()-m_time)/500.0f;
	if (m_buildRedMultiplier>4.0f || m_buildRedMultiplier<0) {
		m_buildRedMultiplier = 0;
	}

	render();

	TheFramePacer->update();

	m_time = ::GetTickCount();
}

// ----------------------------------------------------------------------------
void WbView3d::render()
{
	++m_updateCount;

	if (WW3D::Begin_Render(true,true,Vector3(0.5f,0.5f,0.5f), TheWaterTransparency->m_minWaterOpacity) == WW3D_ERROR_OK)
	{

		DEBUG_ASSERTCRASH((m_heightMapRenderObj),("oops"));


		if (m_heightMapRenderObj) {
			m_heightMapRenderObj->Set_Hidden((m_showTerrain ? 0 : 1));
			m_heightMapRenderObj->doTextures(true);
		}
		m_scene->Set_Polygon_Mode(SceneClass::FILL);
		// Render 3D scene
		WW3D::Render(m_scene,m_camera);
		Vector3 amb = m_baseBuildScene->Get_Ambient_Light();
		Vector3 newAmb(amb);
		Real mul = m_buildRedMultiplier;
		if (mul>2.0f) mul = 4.0f-mul;
		Real gMul = 2.0-mul;
		newAmb.X *= mul;
		newAmb.Y *= gMul;
		if (newAmb.X>1) newAmb.X = 1;
		m_baseBuildScene->Set_Ambient_Light(newAmb);
		WW3D::Render(m_baseBuildScene,m_camera);
		m_baseBuildScene->Set_Ambient_Light(amb);

		if (m_showWireframe) {
			if (m_heightMapRenderObj) {
				m_heightMapRenderObj->doTextures(false);
				m_scene->Set_Polygon_Mode(SceneClass::LINE);
				// Render 3D scene
				WW3D::Render(m_scene,m_camera);
				WW3D::Render(m_baseBuildScene,m_camera);
				m_heightMapRenderObj->doTextures(true);
			}
		}
		if (m_showObjToolTrackingObj && m_objectToolTrackingObj) {
			m_transparentObjectsScene->Add_Render_Object(m_objectToolTrackingObj);
			DX8TextureCategoryClass::SetForceMultiply(true);
			TheDX8MeshRenderer.Enable_Lighting(false);
			Real lightLevel = 1.0f;
			m_transparentObjectsScene->Set_Ambient_Light(Vector3(lightLevel,lightLevel,lightLevel));
			WW3D::Render(m_transparentObjectsScene, m_camera);
			TheDX8MeshRenderer.Enable_Lighting(true);
			DX8TextureCategoryClass::SetForceMultiply(false);
			m_transparentObjectsScene->Remove_Render_Object(m_objectToolTrackingObj);
		}

		// Draw the 3d obj icons on top of the rest of the data.
		WW3D::Render(m_overlayScene,m_camera);
		//if (mytext) mytext->Render();
		if (m3DFont) {
			drawLabels(nullptr);
		}


		WW3D::End_Render();
	}
	--m_updateCount;
}

// ----------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(WbView3d, WbView)
	//{{AFX_MSG_MAP(WbView3d)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_MOUSEWHEEL()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_WM_SHOWWINDOW()
	ON_COMMAND(ID_VIEW_SHOWWIREFRAME, OnViewShowwireframe)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWWIREFRAME, OnUpdateViewShowwireframe)
	ON_WM_ERASEBKGND()
	ON_COMMAND(ID_VIEW_SHOWENTIRE3DMAP, OnViewShowentire3dmap)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWENTIRE3DMAP, OnUpdateViewShowentire3dmap)
	ON_COMMAND(ID_VIEW_SHOWTOPDOWNVIEW, OnViewShowtopdownview)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWTOPDOWNVIEW, OnUpdateViewShowtopdownview)
	ON_COMMAND(ID_VIEW_SHOWCLOUDS, OnViewShowclouds)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWCLOUDS, OnUpdateViewShowclouds)
	ON_COMMAND(ID_VIEW_SHOWMACROTEXTURE, OnViewShowmacrotexture)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWMACROTEXTURE, OnUpdateViewShowmacrotexture)
	ON_COMMAND(ID_EDIT_SELECTMACROTEXTURE, OnEditSelectmacrotexture)
	ON_COMMAND(ID_LOOK_EAST, OnLookEast)
	ON_UPDATE_COMMAND_UI(ID_LOOK_EAST, OnUpdateLookEast)
	ON_COMMAND(ID_LOOK_NORTH, OnLookNorth)
	ON_UPDATE_COMMAND_UI(ID_LOOK_NORTH, OnUpdateLookNorth)
	ON_COMMAND(ID_LOOK_SOUTH, OnLookSouth)
	ON_UPDATE_COMMAND_UI(ID_LOOK_SOUTH, OnUpdateLookSouth)
	ON_COMMAND(ID_LOOK_WEST, OnLookWest)
	ON_UPDATE_COMMAND_UI(ID_LOOK_WEST, OnUpdateLookWest)
	ON_COMMAND(ID_VIEW_SHOWSOFTWATER, OnViewShowSoftWater)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWEXTRABLENDS, OnUpdateViewShowExtraBlends)
	ON_COMMAND(ID_VIEW_SHOWEXTRABLENDS, OnViewExtraBlends)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWSOFTWATER, OnUpdateViewShowSoftWater)
	ON_COMMAND(ID_VIEW_SHOWSHADOWS, OnViewShowshadows)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWSHADOWS, OnUpdateViewShowshadows)
	ON_COMMAND(ID_EDIT_SHADOWS, OnEditShadows)
	ON_COMMAND(ID_EDIT_MAPSETTINGS, OnEditMapSettings)
	ON_COMMAND(ID_VIEW_SHOWIMPASSABLEAREAS, OnViewShowimpassableareas)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWIMPASSABLEAREAS, OnUpdateViewShowimpassableareas)
	ON_COMMAND(ID_VIEW_IMPASSABLEAREAOPTIONS, OnImpassableAreaOptions)
	ON_COMMAND(ID_VIEW_PARTIALMAPSIZE_96X96, OnViewPartialmapsize96x96)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PARTIALMAPSIZE_96X96, OnUpdateViewPartialmapsize96x96)
	ON_COMMAND(ID_VIEW_PARTIALMAPSIZE_192X192, OnViewPartialmapsize192x192)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PARTIALMAPSIZE_192X192, OnUpdateViewPartialmapsize192x192)
	ON_COMMAND(ID_VIEW_PARTIALMAPSIZE_160X160, OnViewPartialmapsize160x160)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PARTIALMAPSIZE_160X160, OnUpdateViewPartialmapsize160x160)
	ON_COMMAND(ID_VIEW_PARTIALMAPSIZE_128X128, OnViewPartialmapsize128x128)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PARTIALMAPSIZE_128X128, OnUpdateViewPartialmapsize128x128)
	ON_COMMAND(ID_VIEW_SHOWMODELS, OnViewShowModels)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWMODELS, OnUpdateViewShowModels)
	ON_COMMAND(ID_VIEW_GARRISONED, OnViewGarrisoned)
	ON_UPDATE_COMMAND_UI(ID_VIEW_GARRISONED, OnUpdateViewGarrisoned)
	ON_COMMAND(ID_VIEW_LAYERS_LIST, OnViewLayersList)
	ON_UPDATE_COMMAND_UI(ID_VIEW_LAYERS_LIST, OnUpdateViewLayersList)
	ON_COMMAND(ID_VIEW_SHOWMAPBOUNDARIES, OnViewShowMapBoundaries)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWMAPBOUNDARIES, OnUpdateViewShowMapBoundaries)
	ON_COMMAND(ID_VIEW_SHOWAMBIENTSOUNDS, OnViewShowAmbientSounds)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWAMBIENTSOUNDS, OnUpdateViewShowAmbientSounds)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// ----------------------------------------------------------------------------
// WbView3d drawing

void WbView3d::OnDraw(CDC* pDC)
{
	// Not used.  See OnPaint.
}

// ----------------------------------------------------------------------------
// WbView3d diagnostics

#ifdef RTS_DEBUG
// ----------------------------------------------------------------------------
void WbView3d::AssertValid() const
{
	WbView::AssertValid();
}

// ----------------------------------------------------------------------------
void WbView3d::Dump(CDumpContext& dc) const
{
	WbView::Dump(dc);
}
#endif //RTS_DEBUG

// ----------------------------------------------------------------------------
void WbView3d::initWW3D()
{
	// only want to do once per instance, but do lazily.
	if (!m_ww3dInited) {



		m_ww3dInited = true;

		WWMath::Init();

		WW3D::Set_Prelit_Mode(WW3D::PRELIT_MODE_VERTEX);

		initAssets();
		WW3D::Init(m_hWnd);
		WW3D::Set_Prelit_Mode( WW3D::PRELIT_MODE_LIGHTMAP_MULTI_PASS );
		WW3D::Set_Collision_Box_Display_Mask(0x00);	///<set to 0xff to make collision boxes visible

		bogusTacticalView.setWidth(m_actualWinSize.x);
		bogusTacticalView.setHeight(m_actualWinSize.y);
		bogusTacticalView.setOrigin(0,0);
		if (WW3D::Set_Render_Device(0, m_actualWinSize.x, m_actualWinSize.y, 32, true, true) != WW3D_ERROR_OK)
		{
			// Getting the device at the default bit depth (32) didn't work, so try
			// getting a 16 bit display.  (Voodoo 1-3 only supported 16 bit.) jba.
			if (WW3D::Set_Render_Device(0, m_actualWinSize.x, m_actualWinSize.y, 16, true, true) != WW3D_ERROR_OK)
			{
				DEBUG_CRASH(("Couldn't set render device."));
			}
		}

		IDirect3DDevice8* pDev = DX8Wrapper::_Get_D3D_Device8();
		if (pDev) {

//			CDC* pDC = GetDC();
			LOGFONT logFont;
			logFont.lfHeight = 20;
			logFont.lfWidth = 0;
			logFont.lfEscapement = 0;
			logFont.lfOrientation = 0;
			logFont.lfWeight = FW_REGULAR;
			logFont.lfItalic = FALSE;
			logFont.lfUnderline = FALSE;
			logFont.lfStrikeOut = FALSE;
			logFont.lfCharSet = ANSI_CHARSET;
			logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
			logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
			logFont.lfQuality = DEFAULT_QUALITY;
			logFont.lfPitchAndFamily = DEFAULT_PITCH;
			strcpy(logFont.lfFaceName, "Arial");

			HFONT hFont = CreateFontIndirect(&logFont);
			if (hFont) {
				D3DXCreateFont(pDev, hFont, &m3DFont);
				DeleteObject(hFont);
			} else {
				m3DFont = nullptr;
			}

		} else {
			m3DFont = nullptr;
		}

		WW3D::Enable_Static_Sort_Lists(true);
		WW3D::Set_Thumbnail_Enabled(false);
		WW3D::Set_Screen_UV_Bias( TRUE );  ///< this makes text look good :)

		W3DShaderManager::init();
		init3dScene();
		m_layer = new LayerClass( m_scene, m_camera );
		m_buildLayer = new LayerClass( m_baseBuildScene, m_camera );
		m_intersector = new IntersectionClass();
		m_drawObject = new DrawObject();
		m_overlayScene->Add_Render_Object(m_drawObject);

#if 1
		TheWritableGlobalData->m_useShadowVolumes = true;
		TheWritableGlobalData->m_useShadowDecals = true;
		TheWritableGlobalData->m_enableBehindBuildingMarkers = false;	//this is only for the game.
		if (TheW3DShadowManager==nullptr)
		{	TheW3DShadowManager = new W3DShadowManager;
 			TheW3DShadowManager->init();
		}
#endif
		updateLights();
		resetRenderObjects();
	}
}

// ----------------------------------------------------------------------------
// WbView3d message handlers

// ----------------------------------------------------------------------------
int WbView3d::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (WbView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// install debug callbacks
	WWDebug_Install_Message_Handler(WWDebug_Message_Callback);
	WWDebug_Install_Assert_Handler(WWAssert_Callback);

	m_timer = SetTimer(0, UPDATE_TIME, nullptr);

	initWW3D();
	TheWritableGlobalData->m_useCloudMap = AfxGetApp()->GetProfileInt("GameOptions", "cloudMap", 0);
	AfxGetApp()->WriteProfileInt("GameOptions", "cloudMap", TheGlobalData->m_useCloudMap);	// Just in case it wasn't already there
 	m_partialMapSize = AfxGetApp()->GetProfileInt("GameOptions", "partialMapSize", 97);

	m_showLayersList = AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowLayersList", 0);
	m_showMapBoundaries = AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowMapBoundaries", 0);
	m_showAmbientSounds = AfxGetApp()->GetProfileInt(MAIN_FRAME_SECTION, "ShowAmbientSounds", 0);

	DrawObject::setDoBoundaryFeedback(m_showMapBoundaries);
	DrawObject::setDoAmbientSoundFeedback(m_showAmbientSounds);
	return 0;
}

// ----------------------------------------------------------------------------
void WbView3d::OnPaint()
{

	PAINTSTRUCT ps;
	HDC hdc = ::BeginPaint(m_hWnd, &ps);
	if (!m_firstPaint) {
		redraw();
	}
	drawLabels(hdc);
	::EndPaint(m_hWnd, &ps);
	if (m_firstPaint) {
		CMainFrame::GetMainFrame()->adjustWindowSize();
		m_firstPaint = false;
	}
	DX8Wrapper::SetCleanupHook(this);

}


void WbView3d::drawLabels()
{
	CDC * pDC = GetDC();
	drawLabels(pDC->m_hDC);
	ReleaseDC(pDC);
}

/// This is actually draw any 2d graphics and/or feedback.
void WbView3d::drawLabels(HDC hdc)
{
	Coord3D selectedPos;	//position of selected object
	Real	selectedRadius=120.0f;	//default distance of lightfeeback model from object
	selectedPos.x=0;selectedPos.y=0;selectedPos.z=0;

	// Draw labels.
	MapObject *pMapObj;
	if (isNamesVisible())
	{
		for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
			AsciiString name;
			Coord3D pos;

			if (pMapObj->getFlags() & FLAG_DONT_RENDER) {
				continue;
			}

			if (m_doLightFeedback && pMapObj->isSelected())
			{	//find out position of selected object in order to use it for light feedback tracking.
				selectedPos=*pMapObj->getLocation();
				selectedPos.z = m_heightMapRenderObj->getHeightMapHeight(selectedPos.x, selectedPos.y, nullptr);
				RenderObjClass *selRobj=pMapObj->getRenderObj();
				if (selRobj)
				{
					SphereClass sphere;
					selRobj->Get_Obj_Space_Bounding_Sphere(sphere);
					selectedRadius=sphere.Radius + sphere.Center.Length()+20.0f;
				}

			}

			if (pMapObj->isWaypoint() && m_showWaypoints) {
				name = pMapObj->getWaypointName();
				pos = *pMapObj->getLocation();
				pos.z = m_heightMapRenderObj->getHeightMapHeight(pos.x, pos.y, nullptr);
			} else if (pMapObj->getThingTemplate() && !(pMapObj->getFlags() & (FLAG_ROAD_FLAGS|FLAG_BRIDGE_FLAGS)) &&
								 pMapObj->getRenderObj() == nullptr) {
				name = pMapObj->getThingTemplate()->getName();
				pos = *pMapObj->getLocation();
				pos.z += m_heightMapRenderObj->getHeightMapHeight(pos.x, pos.y, nullptr);
			}
			Int i;
			for (i=0; i<4; i++) {
				Bool exists;
				switch(i) {
					case 0 : break;
					case 1: name = pMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel1, &exists); break;
					case 2: name = pMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel2, &exists); break;
					case 3: name = pMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel3, &exists); break;
					default: name.clear();
				}
				if (!name.isEmpty()) {
					CPoint pt;
					Vector3 world, screen;
					world.Set( pos.x+MAP_XY_FACTOR/2, pos.y, pos.z );
					if (CameraClass::INSIDE_FRUSTUM != m_camera->Project( screen, world )) {
						continue;
					}
					if (!name.isEmpty()) {
						CPoint pt;
						Vector3 world, screen;
						world.Set( pos.x+MAP_XY_FACTOR/2, pos.y, pos.z );
						if (CameraClass::INSIDE_FRUSTUM != m_camera->Project( screen, world )) {
							continue;
						}

						CRect rClient;
						GetClientRect(&rClient);

						//
						// note that the screen coord returned from the project W3D camera
						// gave us a screen coords that range from (-1,-1) bottom left to
						// (1,1) top right ... we are turning that into (0,0) upper left
						// coords now
						//
						Int sx, sy;
						W3DLogicalScreenToPixelScreen( screen.X, screen.Y,
																					 &sx, &sy,
																					 rClient.right-rClient.left, rClient.bottom-rClient.top );
						pt.x = rClient.left+sx;
						pt.y = rClient.top+sy;
						pt.y += i*15;

						Int red, green;
						if (i==0) {
							red = 0; green = 255;
						} else {
							red = 255, green = 0;
						}

						if (m3DFont && !hdc) {
							RECT rct;
							pt.y -= 5;
							pt.x += 1;
							rct.top = rct.bottom = pt.y;
							rct.left = rct.right = pt.x;
							m3DFont->DrawText(name.str(), name.getLength(), &rct,
								DT_LEFT | DT_NOCLIP | DT_TOP | DT_SINGLELINE, 0xAF000000 + (red<<16) + (green<<8));

						} else if (!m3DFont) {
							//docToViewCoords(pos, &pt);
							::SetBkMode(hdc, TRANSPARENT);
							pt.y -= 5;
							pt.x += 1;
							::SetTextColor(hdc, RGB(red,green,0));
							::TextOut(hdc, pt.x, pt.y, name.str(), name.getLength());
						}
					}
				}
			}
		}
	}

	// Draw tracking box.
	if (hdc && m_doRectFeedback) {
		CBrush brush;
		// green brush for drawing the grid.
		brush.CreateSolidBrush(RGB(0,255,0));
		::FrameRect(hdc, &m_feedbackBox, (HBRUSH)brush.GetSafeHandle());
	}

	if (hdc && m_doLightFeedback)
	{	//Draw Lines to indicate the direction of each light source
//		Int LightColors[MAX_GLOBAL_LIGHTS]={RGB(255,0,0),RGB(0,255,0),RGB(0,0,255)};

		for (Int lIndex=0; lIndex<MAX_GLOBAL_LIGHTS; lIndex++)
		{
			Vector3 worldStart, screenStart;	//start of line
			Vector3 worldEnd,	screenEnd;		//end of line

			worldStart.Set( selectedPos.x, selectedPos.y, selectedPos.z );
			worldEnd.Set(selectedPos.x - m_lightDirection[lIndex].x*selectedRadius,
						selectedPos.y - m_lightDirection[lIndex].y*selectedRadius,
						selectedPos.z - m_lightDirection[lIndex].z*selectedRadius);

			if (m_lightFeedbackMesh[lIndex] == nullptr)
			{	char nameBuf[64];
				snprintf(nameBuf, ARRAY_SIZE(nameBuf), "WB_LIGHT%d", lIndex+1);
				m_lightFeedbackMesh[lIndex]=WW3DAssetManager::Get_Instance()->Create_Render_Obj(nameBuf);
			}
			if (m_lightFeedbackMesh[lIndex]==nullptr) {
				break;
			}
			Matrix3D lightMat;

			lightMat.Look_At(worldEnd,worldStart,0);
			lightMat.Set_Translation(worldEnd);

			m_lightFeedbackMesh[lIndex]->Add(m_scene);
			m_lightFeedbackMesh[lIndex]->Set_Transform(lightMat);
#ifdef DRAW_LIGHT_DIRECTION_RAYS
			if (CameraClass::INSIDE_FRUSTUM == m_camera->Project( screenStart, worldStart ) &&
				CameraClass::INSIDE_FRUSTUM == m_camera->Project( screenEnd, worldEnd ))
			{
				CRect rClient;
				GetClientRect(&rClient);

				//
				// note that the screen coord returned from the project W3D camera
				// gave us a screen coords that range from (-1,-1) bottom left to
				// (1,1) top right ... we are turning that into (0,0) upper left
				// coords now
				//
				Int sxStart, syStart;
				Int sxEnd, syEnd;

				W3DLogicalScreenToPixelScreen( screenStart.X, screenStart.Y, &sxStart, &syStart,rClient.right-rClient.left, rClient.bottom-rClient.top );
				W3DLogicalScreenToPixelScreen( screenEnd.X, screenEnd.Y, &sxEnd, &syEnd,rClient.right-rClient.left, rClient.bottom-rClient.top );

				POINT rayPoints[2];

				rayPoints[0].x = rClient.left+sxStart;
				rayPoints[0].y= rClient.top+syStart;

				rayPoints[1].x = rClient.left+sxEnd;
				rayPoints[1].y= rClient.top+syEnd;

				HPEN pen=CreatePen( PS_SOLID,2, LightColors[lIndex]);
				HPEN penOld = (HPEN)SelectObject(hdc, pen);
				Polyline(hdc,rayPoints,2);
				SelectObject(hdc, penOld);	//restore previous pen
				DeleteObject(pen);	//delete new pen
			}
#endif	//DRAW_LIGHT_DIRECTION_RAYS
		}
	}
	else
	{	if (!m_doLightFeedback)
		{	//not in light feedback mode.  Make sure the temporary feeback models are gone

			for (Int lIndex=0; lIndex<MAX_GLOBAL_LIGHTS; lIndex++)
			{
				if (m_lightFeedbackMesh[lIndex] != nullptr)
				{	m_lightFeedbackMesh[lIndex]->Remove();
					REF_PTR_RELEASE(m_lightFeedbackMesh[lIndex]);
				}
			}
		}
	}
}


// ----------------------------------------------------------------------------
void WbView3d::OnSize(UINT nType, int cx, int cy)
{
	WbView::OnSize(nType, cx, cy);

}

// ----------------------------------------------------------------------------
BOOL WbView3d::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (m_trackingMode == TRACK_NONE) {
		m_mouseWheelOffset += zDelta;
		MSG msg;
		while (::PeekMessage(&msg, m_hWnd, WM_MOUSEWHEEL, WM_MOUSEWHEEL, PM_REMOVE)) {
			zDelta = (short) HIWORD(msg.wParam);    // wheel rotation
			m_mouseWheelOffset += zDelta;
		}
		redraw();
		updateHysteresis();
		drawLabels();
		CMainFrame::GetMainFrame()->handleCameraChange();
	}
	return true;
}

// ----------------------------------------------------------------------------
void WbView3d::setDefaultCamera()
{

	m_mouseWheelOffset = 0;
	m_cameraAngle = 0;
	m_FXPitch = 1.0f;
	if (m_centerPt.X < 0) m_centerPt.X = 0;
	if (m_centerPt.Y < 0) m_centerPt.Y = 0;
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc) {
		WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
		if (pMap) {
			if (m_centerPt.X > pMap->getXExtent()) {
				m_centerPt.X = pMap->getXExtent();
			}
			if (m_centerPt.Y > pMap->getYExtent()) {
				m_centerPt.Y = pMap->getYExtent();
			}
		}
	}
	m_groundLevel = 10.0;
	Coord3D pos;
	pos.x = m_centerPt.X;
	pos.y = m_centerPt.Y;
	pos.z = m_centerPt.Z;
	AsciiString startingCamName = TheNameKeyGenerator->keyToName(TheKey_InitialCameraPosition);
	MapObject *pMapObj = MapObject::getFirstMapObject();
	while (pMapObj) {
		if (pMapObj->isWaypoint()) {
			if (startingCamName == pMapObj->getWaypointName()) {
				pos = *pMapObj->getLocation();
			}
		}
		pMapObj = pMapObj->getNext();
	}

	if (m_heightMapRenderObj) {
		m_groundLevel = m_heightMapRenderObj->getHeightMapHeight(pos.x, pos.y, nullptr);
	}

	//m_cameraOffset.z = m_groundLevel+TheGlobalData->m_cameraHeight;
	m_cameraOffset.z = m_groundLevel+TheGlobalData->m_maxCameraHeight;
	m_cameraOffset.y = -(m_cameraOffset.z / tan(TheGlobalData->m_cameraPitch * (PI / 180.0)));
	m_cameraOffset.x = -(m_cameraOffset.y * tan(TheGlobalData->m_cameraYaw * (PI / 180.0)));

	m_cameraOffset.x *= 1.0f;
	m_cameraOffset.y *= 1.0f;
	m_cameraOffset.z *= 1.0f;
	/*
	m_cameraOffset.x *= TheGlobalData->m_maxZoom;
	m_cameraOffset.y *= TheGlobalData->m_maxZoom;
	m_cameraOffset.z *= TheGlobalData->m_maxZoom;
	*/

	redraw();
	drawLabels();
	CMainFrame::GetMainFrame()->handleCameraChange();
}

// ----------------------------------------------------------------------------
void WbView3d::rotateCamera(Real delta)
{
	if (m_projection) return; // camera doesn't rotate in top down view.
	m_cameraAngle += delta;
	redraw();
	drawLabels();
	CMainFrame::GetMainFrame()->handleCameraChange();
}

// ----------------------------------------------------------------------------
void WbView3d::pitchCamera(Real delta)
{
	if (m_projection) return; // camera doesn't pitch in top down view.
	m_FXPitch += delta;
	redraw();
	drawLabels();
	CMainFrame::GetMainFrame()->handleCameraChange();
}

// ----------------------------------------------------------------------------
void WbView3d::setCameraPitch(Real absolutePitch)
{
	if (m_projection) return; // camera doesn't pitch in top down view.
	m_FXPitch = absolutePitch;
	redraw();
	drawLabels();
	CMainFrame::GetMainFrame()->handleCameraChange();
}

// ----------------------------------------------------------------------------
Real WbView3d::getCameraPitch()
{
	return m_FXPitch;
}

// ----------------------------------------------------------------------------
void WbView3d::OnTimer(UINT nIDEvent)
{
	if (getLastDrawTime()+UPDATE_TIME<::GetTickCount())
	{
		Invalidate(false);
	}
}

// ----------------------------------------------------------------------------
void WbView3d::OnDestroy()
{
	killTheTimer();
	WbView::OnDestroy();
}

// ----------------------------------------------------------------------------
void WbView3d::OnShowWindow(BOOL bShow, UINT nStatus)
{
	WbView::OnShowWindow(bShow, nStatus);
}

//=============================================================================
// CWorldBuilderView::scroll
//=============================================================================
/** Scrolls the window. */
//=============================================================================
void WbView3d::scrollInView(Real xScroll, Real yScroll, Bool end)
{
	m_centerPt.X += xScroll;
	m_centerPt.Y += yScroll;
	constrainCenterPt();
	redraw();
	drawLabels();
	if (end)
		WbDoc()->syncViewCenters(m_centerPt.X, m_centerPt.Y);
	CMainFrame::GetMainFrame()->handleCameraChange();
}

void WbView3d::OnViewShowwireframe()
{
	m_showWireframe = !m_showWireframe;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowWireframe", m_showWireframe?1:0);
}

void WbView3d::OnUpdateViewShowwireframe(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showWireframe?1:0);

}

BOOL WbView3d::OnEraseBkgnd(CDC* pDC)
{
	// Never erase the background.  The 3d view always draws the entire
	// window, so erasing just makes it flicker.  jba.
	return true; // act like we erased.
}

void WbView3d::OnViewShowentire3dmap()
{
	m_showEntireMap = !m_showEntireMap;
	IRegion2D range = {0,0,0,0};
	this->updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	Invalidate(false);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowEntireMap", m_showEntireMap?1:0);
}

void WbView3d::OnUpdateViewShowentire3dmap(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showEntireMap?1:0);
}

void WbView3d::OnViewShowtopdownview()
{
	m_projection = !m_projection;
	m_heightMapRenderObj->setFlattenHeights(m_projection);
	invalObjectInView(nullptr);
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowTopDownView", m_projection?1:0);
}

void WbView3d::OnUpdateViewShowtopdownview(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_projection?1:0);
}

void WbView3d::OnViewShowclouds()
{
	TheWritableGlobalData->m_useCloudMap = !TheGlobalData->m_useCloudMap;
	AfxGetApp()->WriteProfileInt("GameOptions", "cloudMap", TheGlobalData->m_useCloudMap);
}

void WbView3d::OnUpdateViewShowclouds(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(TheGlobalData->m_useCloudMap?1:0);
}

void WbView3d::OnViewShowmacrotexture()
{
	Bool show = !TheGlobalData->m_useLightMap;
	TheWritableGlobalData->m_useLightMap = show;
}

void WbView3d::OnUpdateViewShowmacrotexture(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(TheGlobalData->m_useLightMap?1:0);
}

void WbView3d::OnEditSelectmacrotexture()
{
	SelectMacrotexture dlg;

	// The macrotexture dialog sets the macrotexture in the 3d engine.
	dlg.DoModal();

}

void WbView3d::OnLookEast()
{
	m_cameraAngle = -PI/2;
}

void WbView3d::OnUpdateLookEast(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_cameraAngle==-PI/2?1:0);
}

void WbView3d::OnLookNorth()
{
	m_cameraAngle = 0;
}

void WbView3d::OnUpdateLookNorth(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_cameraAngle==0?1:0);
}

void WbView3d::OnLookSouth()
{
	m_cameraAngle = PI;

}

void WbView3d::OnUpdateLookSouth(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_cameraAngle==PI?1:0);
}

void WbView3d::OnLookWest()
{
	m_cameraAngle = PI/2;
}

void WbView3d::OnUpdateLookWest(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_cameraAngle==PI/2?1:0);
}

void WbView3d::OnViewShowshadows()
{
	m_showShadows = !m_showShadows;
	if (m_showShadows) {
		int w,h,bits;
		Bool windowed;
		WW3D::Get_Device_Resolution(w,h,bits,windowed);

		if (bits != 32) {
			::AfxMessageBox("Shadows require a 32 bit color desktop.", IDOK);
			m_showShadows = false;
		} else {
			resetRenderObjects();
			invalObjectInView(nullptr);
		}
	} else {
		TheW3DShadowManager->removeAllShadows();
	}
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowShadows", m_showShadows?1:0);
}

void WbView3d::OnUpdateViewShowshadows(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showShadows?1:0);
}

void WbView3d::OnViewShowSoftWater()
{
	TheWritableGlobalData->m_showSoftWaterEdge = !TheGlobalData->m_showSoftWaterEdge;
	if (TheGlobalData->m_showSoftWaterEdge)
	{	//we just turned it on, so recompute shoreline tiles since they may not exist.
		TheTerrainRenderObject->updateShorelineTiles(0,0,WbDoc()->GetHeightMap()->getXExtent()-1,WbDoc()->GetHeightMap()->getYExtent()-1,
			WbDoc()->GetHeightMap());
	}

	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowSoftWater", TheGlobalData->m_showSoftWaterEdge ? 1 : 0);
}

void WbView3d::OnUpdateViewShowSoftWater(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(TheGlobalData->m_showSoftWaterEdge ? 1 : 0);
}

void WbView3d::OnViewExtraBlends()
{
	if (TheGlobalData->m_use3WayTerrainBlends==1)
		TheWritableGlobalData->m_use3WayTerrainBlends = 2;	//debug mode which draws the tiles in black
	else
		TheWritableGlobalData->m_use3WayTerrainBlends = 1;

	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowExtraBlends", TheGlobalData->m_use3WayTerrainBlends > 1 ? 1 : 0);
}

void WbView3d::OnUpdateViewShowExtraBlends(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(TheGlobalData->m_use3WayTerrainBlends > 1 ? 1 : 0);
}

void WbView3d::OnEditMapSettings()
{
	MapSettings dlg;

	if (dlg.DoModal() == IDOK) {
		resetRenderObjects();
		invalObjectInView(nullptr);
	}
}

void WbView3d::OnEditShadows()
{
	if (!m_showShadows) {
		OnViewShowshadows(); // turn them on.
	}
	ShadowOptions dlg;
	dlg.DoModal();
}

void WbView3d::OnViewShowModels()
{
	setShowModels(!getShowModels());
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowModels", getShowModels()?1:0);
	resetRenderObjects();
	invalObjectInView(nullptr);
}
void WbView3d::OnUpdateViewShowModels(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(getShowModels()?1:0);
}

void WbView3d::OnViewGarrisoned()
{
	setShowGarrisoned(!getShowGarrisoned());
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowGarrisoned", getShowGarrisoned()?1:0);
	resetRenderObjects();
	invalObjectInView(nullptr);
}
void WbView3d::OnUpdateViewGarrisoned(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(getShowGarrisoned()?1:0);
}

void WbView3d::OnViewShowimpassableareas()
{
	Bool showImpassable = false;
	if (TheTerrainRenderObject) {
		showImpassable = TheTerrainRenderObject->getShowImpassableAreas();
		TheTerrainRenderObject->setShowImpassableAreas(!showImpassable);
		// Force the entire terrain mesh to be rerendered.
		IRegion2D range = {0,0,0,0};
		updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	}
}

void WbView3d::OnUpdateViewShowimpassableareas(CCmdUI* pCmdUI)
{
	Bool showImpassable = false;
	if (TheTerrainRenderObject) {
		showImpassable = TheTerrainRenderObject->getShowImpassableAreas();
	}
	pCmdUI->SetCheck(showImpassable?1:0);
}

void WbView3d::OnImpassableAreaOptions()
{
	if (TheTerrainRenderObject) {
		{
			ImpassableOptions opts;
			opts.SetDefaultSlopeToShow(TheTerrainRenderObject->getViewImpassableAreaSlope());
			if (opts.DoModal() == IDOK) {
				TheTerrainRenderObject->setViewImpassableAreaSlope(opts.GetSlopeToShow());
			} else {
				TheTerrainRenderObject->setViewImpassableAreaSlope(opts.GetDefaultSlope());
			}
		}

		IRegion2D range = {0,0,0,0};
		updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	}
}

void WbView3d::OnViewPartialmapsize96x96()
{
	m_partialMapSize = 97;
	AfxGetApp()->WriteProfileInt("GameOptions", "partialMapSize", m_partialMapSize);
	m_showEntireMap = false;
	IRegion2D range = {0,0,0,0};
	WbDoc()->GetHeightMap()->setDrawOrg(0, 0);
	updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	Invalidate(false);
}

void WbView3d::OnUpdateViewPartialmapsize96x96(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_partialMapSize == 97?1:0);
}

void WbView3d::OnViewPartialmapsize192x192()
{
	m_partialMapSize = 192;
	AfxGetApp()->WriteProfileInt("GameOptions", "partialMapSize", m_partialMapSize);
	m_showEntireMap = false;
	IRegion2D range = {0,0,0,0};
	WbDoc()->GetHeightMap()->setDrawOrg(0, 0);
	updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	Invalidate(false);
}

void WbView3d::OnUpdateViewPartialmapsize192x192(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_partialMapSize == 192?1:0);
}

void WbView3d::OnViewPartialmapsize160x160()
{
	m_partialMapSize = 161;
	AfxGetApp()->WriteProfileInt("GameOptions", "partialMapSize", m_partialMapSize);
	m_showEntireMap = false;
	IRegion2D range = {0,0,0,0};
	WbDoc()->GetHeightMap()->setDrawOrg(0, 0);
	updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	Invalidate(false);
}

void WbView3d::OnUpdateViewPartialmapsize160x160(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_partialMapSize == 161?1:0);
}

void WbView3d::OnViewPartialmapsize128x128()
{
	m_partialMapSize = 129;
	AfxGetApp()->WriteProfileInt("GameOptions", "partialMapSize", m_partialMapSize);
	m_showEntireMap = false;
	IRegion2D range = {0,0,0,0};
	WbDoc()->GetHeightMap()->setDrawOrg(0, 0);
	updateHeightMapInView(WbDoc()->GetHeightMap(), false, range);
	Invalidate(false);
}

void WbView3d::OnUpdateViewPartialmapsize128x128(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_partialMapSize == 129?1:0);
}

void WbView3d::OnViewLayersList()
{
	m_showLayersList = !m_showLayersList;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowLayersList", m_showLayersList ? 1 : 0);
	TheLayersList->ShowWindow(m_showLayersList ? SW_SHOW : SW_HIDE);
	if (m_showLayersList) {
		TheLayersList->enableUpdates();
	} else {
		TheLayersList->disableUpdates();
	}
}

void WbView3d::OnUpdateViewLayersList(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showLayersList ? 1 : 0);
}

void WbView3d::OnViewShowMapBoundaries()
{
	m_showMapBoundaries = !m_showMapBoundaries;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowMapBoundaries", m_showMapBoundaries ? 1 : 0);
	DrawObject::setDoBoundaryFeedback(m_showMapBoundaries);
}

void WbView3d::OnUpdateViewShowMapBoundaries(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showMapBoundaries ? 1 : 0);
}


void WbView3d::OnViewShowAmbientSounds()
{
	m_showAmbientSounds = !m_showAmbientSounds;
	::AfxGetApp()->WriteProfileInt(MAIN_FRAME_SECTION, "ShowAmbientSounds", m_showAmbientSounds ? 1 : 0);
	DrawObject::setDoAmbientSoundFeedback(m_showAmbientSounds);
}

void WbView3d::OnUpdateViewShowAmbientSounds(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_showAmbientSounds ? 1 : 0);
}
