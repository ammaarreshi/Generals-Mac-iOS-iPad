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

// FILE: W3DModelDraw.h /////////////////////////////////////////////////////////////////////////
// Author: Colin Day, November 2001
// Desc:   Default client update module
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/ModelState.h"
#include "Common/DrawModule.h"
#ifdef BRUTAL_TIMING_HACK // hack for collecting model timing info.  jba.
class RenderObjClass {
public:
	enum AnimMode
	{
		ANIM_MODE_MANUAL		= 0,
		ANIM_MODE_LOOP,
		ANIM_MODE_ONCE,
		ANIM_MODE_LOOP_PINGPONG,
		ANIM_MODE_LOOP_BACKWARDS,	//make sure only backwards playing animations after this one
		ANIM_MODE_ONCE_BACKWARDS,

		ANIM_MODE_COUNT
	};
};

#else
#include "WW3D2/rendobj.h"
#endif
#include "Common/SparseMatchFinder.h"
#include "GameClient/ParticleSys.h"
#include "Common/STLTypedefs.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Thing;
class RenderObjClass;
class Shadow;
class TerrainTracksRenderObjClass;
class HAnimClass;
enum GameLODLevel CPP_11(: Int);
//-------------------------------------------------------------------------------------------------
/** The default client update module */
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
typedef UnsignedInt64 TransitionSig;

const TransitionSig NO_TRANSITION = 0;

inline TransitionSig buildTransitionSig(NameKeyType src, NameKeyType dst)
{
	return (((UnsignedInt64)src) << 32) | ((UnsignedInt64)dst);
}

inline NameKeyType recoverSrcState(TransitionSig sig)
{
	return (NameKeyType)((sig >> 32) & 0xffffffff);
}

inline NameKeyType recoverDstState(TransitionSig sig)
{
	return (NameKeyType)((sig) & 0xffffffff);
}

//-------------------------------------------------------------------------------------------------
// please do not define RETAIN_ANIM_HANDLES. It prevents us from ever releasing animations.
// old code should be deleting by mid-Jan 2003. (srj)
#define NO_RETAIN_ANIM_HANDLES

//-------------------------------------------------------------------------------------------------
class W3DAnimationInfo
{
private:
	AsciiString						m_name;
#ifdef RETAIN_ANIM_HANDLES
	mutable HAnimClass*		m_handle;
#endif
	Real									m_distanceCovered;		// if nonzero, the distance covered by a single loop of the anim
	mutable Real					m_naturalDurationInMsec;
	Bool									m_isIdleAnim;					// if true, we pick another animation at random when this one completes

public:
	W3DAnimationInfo(const AsciiString& name, Bool isIdle, Real distanceCovered);
	W3DAnimationInfo(const W3DAnimationInfo &r);
	W3DAnimationInfo &operator=(const W3DAnimationInfo& r);

	~W3DAnimationInfo();

	HAnimClass* getAnimHandle() const;
	const AsciiString& getName() const { return m_name; }
	Bool isIdleAnim() const { return m_isIdleAnim; }
	Real getDistanceCovered() const { return m_distanceCovered; }
	Real getNaturalDurationInMsec() const { return m_naturalDurationInMsec; }

};
typedef std::vector<W3DAnimationInfo>	W3DAnimationVector;

//-------------------------------------------------------------------------------------------------
struct ParticleSysBoneInfo
{
	AsciiString boneName;
	const ParticleSystemTemplate* particleSystemTemplate;
};

typedef std::vector<ParticleSysBoneInfo> ParticleSysBoneInfoVector;

//-------------------------------------------------------------------------------------------------
struct PristineBoneInfo
{
	Matrix3D mtx;
	Int boneIndex;
};
//typedef std::hash_map< NameKeyType, PristineBoneInfo, rts::hash<NameKeyType>, rts::equal_to<NameKeyType> > PristineBoneInfoMap;
typedef std::map< NameKeyType, PristineBoneInfo, std::less<NameKeyType>/**/> PristineBoneInfoMap;

//-------------------------------------------------------------------------------------------------

struct ModelConditionInfo
{
	struct HideShowSubObjInfo
	{
		AsciiString subObjName;
		Bool hide;
	};

	struct TurretInfo
	{
		// read from INI
		NameKeyType		m_turretAngleNameKey;
		NameKeyType		m_turretPitchNameKey;
		Real					m_turretArtAngle;
		Real					m_turretArtPitch;
		// calculated at runtime
		Int						m_turretAngleBone;
		Int						m_turretPitchBone;

		void clear()
		{
			m_turretAngleNameKey = NAMEKEY_INVALID;
			m_turretPitchNameKey = NAMEKEY_INVALID;
			m_turretArtAngle = 0;
			m_turretArtPitch = 0;
			m_turretAngleBone = 0;
			m_turretPitchBone = 0;
		}
	};

	struct WeaponBarrelInfo
	{
		Int							m_recoilBone;									///< the W3D bone for this barrel (zero == no bone)
		Int							m_fxBone;											///< the FX bone for this barrel (zero == no bone)
		Int							m_muzzleFlashBone;						///< the muzzle-flash subobj bone for this barrel (zero == none)
		Matrix3D				m_projectileOffsetMtx;				///< where the projectile fires from
#if defined(RTS_DEBUG) || defined(DEBUG_CRASHING)
		AsciiString			m_muzzleFlashBoneName;
#endif

		WeaponBarrelInfo()
		{
			clear();
		}

		void clear()
		{
			m_recoilBone = 0;
			m_fxBone = 0;
			m_muzzleFlashBone = 0;
			m_projectileOffsetMtx.Make_Identity();
#if defined(RTS_DEBUG) || defined(DEBUG_CRASHING)
			m_muzzleFlashBoneName.clear();
#endif
		}

		void setMuzzleFlashHidden(RenderObjClass *fullObject, Bool hide) const;
	};
	typedef std::vector<WeaponBarrelInfo>	WeaponBarrelInfoVec;

#if defined(RTS_DEBUG) || defined(DEBUG_CRASHING)
	AsciiString												m_description;
#endif
	std::vector<ModelConditionFlags>	m_conditionsYesVec;
	AsciiString												m_modelName;
	std::vector<HideShowSubObjInfo>		m_hideShowVec;
	mutable std::vector<AsciiString>	m_publicBones;
	AsciiString												m_weaponFireFXBoneName[WEAPONSLOT_COUNT];
	AsciiString												m_weaponRecoilBoneName[WEAPONSLOT_COUNT];
	AsciiString												m_weaponMuzzleFlashName[WEAPONSLOT_COUNT];
	AsciiString												m_weaponProjectileLaunchBoneName[WEAPONSLOT_COUNT];
	AsciiString												m_weaponProjectileHideShowName[WEAPONSLOT_COUNT];
	W3DAnimationVector								m_animations;
	NameKeyType												m_transitionKey;
	NameKeyType												m_allowToFinishKey;
	Int																m_flags;
	Int																m_iniReadFlags;	// not read from ini, but used for helping with default states
	RenderObjClass::AnimMode					m_mode;
	ParticleSysBoneInfoVector					m_particleSysBones;			///< Bone names and attached particle systems.
	TransitionSig											m_transitionSig;
	Real															m_animMinSpeedFactor; //Min speed factor (randomized each time it's played)
	Real															m_animMaxSpeedFactor; //Max speed factor (randomized each time it's played)

	mutable PristineBoneInfoMap				m_pristineBones;
	mutable TurretInfo								m_turrets[MAX_TURRETS];
	mutable WeaponBarrelInfoVec				m_weaponBarrelInfoVec[WEAPONSLOT_COUNT];
	mutable Bool											m_hasRecoilBonesOrMuzzleFlashes[WEAPONSLOT_COUNT];
	mutable Byte											m_validStuff;

	enum
	{
		PRISTINE_BONES_VALID		= 0x0001,
		TURRETS_VALID						= 0x0002,
		HAS_PROJECTILE_BONES		= 0x0004,
		BARRELS_VALID						= 0x0008,
		PUBLIC_BONES_VALID			= 0x0010
	};

	ModelConditionInfo()
	{
		clear();
	}

	void clear();
	void loadAnimations() const;
	void preloadAssets( TimeOfDay timeOfDay, Real scale );			///< preload any assets for time of day

	Int getConditionsYesCount() const { DEBUG_ASSERTCRASH(!m_conditionsYesVec.empty(), ("empty m_conditionsYesVec.size(), see srj")); return m_conditionsYesVec.size(); }
	const ModelConditionFlags& getNthConditionsYes(Int i) const { return m_conditionsYesVec[i]; }
#if defined(RTS_DEBUG)
	inline AsciiString getDescription() const { return m_description; }
#endif

	const Matrix3D* findPristineBone(NameKeyType boneName, Int* boneIndex) const;
	Bool findPristineBonePos(NameKeyType boneName, Coord3D& pos) const;
	void addPublicBone(const AsciiString& boneName) const;
	Bool matchesMode(Bool night, Bool snowy) const;

	void validateStuff(RenderObjClass* robj, Real scale, const std::vector<AsciiString>& extraPublicBones) const;

private:
	void validateWeaponBarrelInfo() const;
	void validateTurretInfo() const;
	void validateCachedBones(RenderObjClass* robj, Real scale) const;
};
typedef std::vector<ModelConditionInfo> ModelConditionVector;

//-------------------------------------------------------------------------------------------------
//typedef std::hash_map< TransitionSig, ModelConditionInfo, std::hash<TransitionSig>, std::equal_to<TransitionSig> > TransitionMap;
typedef std::map< TransitionSig, ModelConditionInfo, std::less<TransitionSig>/**/> TransitionMap;

//-------------------------------------------------------------------------------------------------
// this is more efficient and also helps solve a projectile-launch-offset problem for double-upgraded
// technicals. it is not within risk, however, and WILL NOT WORK if the attach-to-module is animated
// in any meaningful way, so be careful. (srj)
#define CACHE_ATTACH_BONE

//-------------------------------------------------------------------------------------------------
class W3DModelDrawModuleData : public ModuleData
{
public:

	mutable ModelConditionVector			m_conditionStates;
	mutable SparseMatchFinder< ModelConditionInfo, ModelConditionFlags >
																		m_conditionStateMap;
	mutable TransitionMap							m_transitionMap;
	std::vector<AsciiString>					m_extraPublicBones;
	AsciiString												m_trackFile;						///< if present, leaves tracks using this texture
	AsciiString												m_attachToDrawableBone;
#ifdef CACHE_ATTACH_BONE
	mutable Vector3										m_attachToDrawableBoneOffset;
#endif
	Int																m_defaultState;
	Int																m_projectileBoneFeedbackEnabledSlots;	///< Hide and show the launch bone geometries according to clip status adjustments.
	Real															m_initialRecoil;
	Real															m_maxRecoil;
	Real															m_recoilDamping;
	Real															m_recoilSettle;
	StaticGameLODLevel								m_minLODRequired;				///< minumum game LOD level necessary to use this module.
	ModelConditionFlags								m_ignoreConditionStates;
	Bool															m_okToChangeModelColor;
	Bool															m_animationsRequirePower;///< Should UnderPowered disable type pause animations in this draw module?
#ifdef CACHE_ATTACH_BONE
	mutable Bool											m_attachToDrawableBoneOffsetValid;
#endif
	mutable Byte											m_validated;

  Bool                              m_particlesAttachedToAnimatedBones;

  Bool                              m_receivesDynamicLights; ///< just like it sounds... it sets a property of Drawable, actually


	W3DModelDrawModuleData();
	virtual ~W3DModelDrawModuleData() override;
	void validateStuffForTimeAndWeather(const Drawable* draw, Bool night, Bool snowy) const;
	static void buildFieldParse(MultiIniFieldParse& p);
 	AsciiString getBestModelNameForWB(const ModelConditionFlags& c) const;
	const ModelConditionInfo* findBestInfo(const ModelConditionFlags& c) const;
	void preloadAssets( TimeOfDay timeOfDay, Real scale ) const;
#ifdef CACHE_ATTACH_BONE
	const Vector3* getAttachToDrawableBoneOffset(const Drawable* draw) const;
#endif

	// ugh, hack
	virtual const W3DModelDrawModuleData* getAsW3DModelDrawModuleData() const override { return this; }
	virtual StaticGameLODLevel getMinimumRequiredGameLOD() const override { return m_minLODRequired;}

private:
	static void parseConditionState( INI* ini, void *instance, void * /*store*/, const void* /*userData*/ );

public:
 	virtual void crc( Xfer *xfer ) override;
 	virtual void xfer( Xfer *xfer ) override;
 	virtual void loadPostProcess() override;
};

//-------------------------------------------------------------------------------------------------
class W3DModelDraw : public DrawModule, public ObjectDrawInterface
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( W3DModelDraw, "W3DModelDraw" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( W3DModelDraw, W3DModelDrawModuleData )

public:

	W3DModelDraw( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

  /// preloading assets
	virtual void preloadAssets( TimeOfDay timeOfDay ) override;

	/// the draw method
	virtual void doDrawModule(const Matrix3D* transformMtx) override;
	virtual void setShadowsEnabled(Bool enable) override;
	virtual void releaseShadows() override;	///< frees all shadow resources used by this module - used by Options screen.
	virtual void allocateShadows() override; ///< create shadow resources if not already present. Used by Options screen.

#if defined(RTS_DEBUG)
	virtual void getRenderCost(RenderCost & rc) const override;  ///< estimates the render cost of this draw module
	void getRenderCostRecursive(RenderCost & rc,RenderObjClass * robj) const;
#endif

	virtual void setFullyObscuredByShroud(Bool fullyObscured) override;
	virtual void setTerrainDecal(TerrainDecalType type) override;

	virtual Bool isVisible() const override;
	virtual void reactToTransformChange(const Matrix3D* oldMtx, const Coord3D* oldPos, Real oldAngle) override;
	virtual void reactToGeometryChange() override { }

	// this method must ONLY be called from the client, NEVER From the logic, not even indirectly.
	virtual Bool clientOnly_getRenderObjInfo(Coord3D* pos, Real* boundingSphereRadius, Matrix3D* transform) const override;
	virtual Bool clientOnly_getRenderObjBoundBox(OBBoxClass * boundbox) const override;
	virtual Bool clientOnly_getRenderObjBoneTransform(const AsciiString & boneName,Matrix3D * set_tm) const override;
	virtual Int getPristineBonePositionsForConditionState(const ModelConditionFlags& condition, const char* boneNamePrefix, Int startIndex, Coord3D* positions, Matrix3D* transforms, Int maxBones) const override;
	virtual Int getCurrentBonePositions(const char* boneNamePrefix, Int startIndex, Coord3D* positions, Matrix3D* transforms, Int maxBones) const override;
	virtual Bool getCurrentWorldspaceClientBonePositions(const char* boneName, Matrix3D& transform) const override;
	virtual Bool getProjectileLaunchOffset(const ModelConditionFlags& condition, WeaponSlotType wslot, Int specificBarrelToUse, Matrix3D* launchPos, WhichTurretType tur, Coord3D* turretRotPos, Coord3D* turretPitchPos = nullptr) const override;
	virtual void updateProjectileClipStatus( UnsignedInt shotsRemaining, UnsignedInt maxShots, WeaponSlotType slot ) override; ///< This will do the show/hide work if ProjectileBoneFeedbackEnabled is set.
	virtual void updateDrawModuleSupplyStatus( Int maxSupply, Int currentSupply ) override; ///< This will do visual feedback on Supplies carried
	virtual void notifyDrawModuleDependencyCleared() override {}///< if you were waiting for something before you drew, it's ready now

	virtual void setHidden(Bool h) override;
	virtual void replaceModelConditionState(const ModelConditionFlags& c) override;
	virtual void replaceIndicatorColor(Color color) override;
	virtual Bool handleWeaponFireFX(WeaponSlotType wslot, Int specificBarrelToUse, const FXList* fxl, Real weaponSpeed, const Coord3D* victimPos, Real damageRadius) override;
	virtual Int getBarrelCount(WeaponSlotType wslot) const override;
	virtual void setSelectable(Bool selectable) override; // Change the selectability of the model.

	/**
		This call says, "I want the current animation (if any) to take n frames to complete a single cycle".
		If it's a looping anim, each loop will take n frames. someday, we may want to add the option to insert
		"pad" frames at the start and/or end, but for now, we always just "stretch" the animation to fit.
		Note that you must call this AFTER setting the condition codes.
	*/
	virtual void setAnimationLoopDuration(UnsignedInt numFrames) override;

	/**
		similar to the above, but assumes that the current state is a "ONCE",
		and is smart about transition states... if there is a transition state
		"in between", it is included in the completion time.
	*/
	virtual void setAnimationCompletionTime(UnsignedInt numFrames) override;

	/**
	  This call is used to pause or resume an animation.
	*/
	virtual void setPauseAnimation(Bool pauseAnim) override;

	//Kris: Manually set a drawable's current animation to specific frame.
	virtual void setAnimationFrame( int frame ) override;

	virtual void updateSubObjects() override;
	virtual void showSubObject( const AsciiString& name, Bool show ) override;

#ifdef ALLOW_ANIM_INQUIRIES
// srj sez: not sure if this is a good idea, for net sync reasons...
	virtual Real getAnimationScrubScalar() const;
#endif

	virtual ObjectDrawInterface* getObjectDrawInterface() override { return this; }
	virtual const ObjectDrawInterface* getObjectDrawInterface() const override { return this; }

	///@todo: I had to make this public because W3DDevice needs access for casting shadows -MW
	RenderObjClass *getRenderObject() { return m_renderObject; }
	virtual Bool updateBonesForClientParticleSystems() override;///< this will reposition particle systems on the fly ML

	virtual void onDrawableBoundToObject() override;
	virtual void setTerrainDecalSize(Real x, Real y) override;
	virtual void setTerrainDecalOpacity(Real o) override;

protected:

	virtual void onRenderObjRecreated(){};

	const ModelConditionInfo* getCurState() const { return m_curState; }

	void setModelState(const ModelConditionInfo* newState);
	const ModelConditionInfo* findBestInfo(const ModelConditionFlags& c) const;
	void handleClientTurretPositioning();
	void handleClientRecoil();
	void recalcBonesForClientParticleSystems();
	void stopClientParticleSystems();
	void doHideShowSubObjs(const std::vector<ModelConditionInfo::HideShowSubObjInfo>* vec);
	virtual void adjustTransformMtx(Matrix3D& mtx) const;

	Real getCurAnimDistanceCovered() const;
	Bool setCurAnimDurationInMsec(Real duration);


	Bool getFullyObscuredByShroud() const { return m_fullyObscuredByShroud; }

private:

	struct WeaponRecoilInfo
	{
		enum RecoilState
		{
			IDLE, RECOIL_START, RECOIL, SETTLE
		};

		RecoilState			m_state;											///< what state this gun is in
		Real						m_shift;											///< how far the gun barrel has recoiled
		Real						m_recoilRate;									///< how fast the gun barrel is recoiling

		WeaponRecoilInfo()
		{
			clear();
		}

		void clear()
		{
			m_state = IDLE;
			m_shift = 0.0f;
			m_recoilRate = 0.0f;
		}
	};


	struct ParticleSysTrackerType
	{
		ParticleSystemID id;
		Int				boneIndex;
	};


	typedef std::vector<WeaponRecoilInfo>	WeaponRecoilInfoVec;
	typedef std::vector<ParticleSysTrackerType>	ParticleSystemIDVec;
	//typedef std::vector<ParticleSystemID>	ParticleSystemIDVec;


	const ModelConditionInfo*			m_curState;
	const ModelConditionInfo*			m_nextState;
	UnsignedInt										m_nextStateAnimLoopDuration;
	Int														m_hexColor;
	Int														m_whichAnimInCurState;						///< the index of the currently playing anim in cur state (if any)
	WeaponRecoilInfoVec						m_weaponRecoilInfoVec[WEAPONSLOT_COUNT];
	Bool													m_needRecalcBoneParticleSystems;
	Bool													m_fullyObscuredByShroud;
	Bool													m_shadowEnabled;	///< cached state of shadow.  Used to determine if shadows should be enabled via options screen.
	RenderObjClass*								m_renderObject;										///< W3D Render object for this drawable
	Shadow*												m_shadow;													///< Updates/Renders shadows of this object
	Shadow*												m_terrainDecal;
	TerrainTracksRenderObjClass*	m_trackRenderObject;							///< This is rendered under object
	ParticleSystemIDVec						m_particleSystemIDs;							///< The ID numbers of the particle systems currently running.
	std::vector<ModelConditionInfo::HideShowSubObjInfo>		m_subObjectVec;
	Bool													m_hideHeadlights;
	Bool													m_pauseAnimation;
	Int														m_animationMode;

	void adjustAnimation(const ModelConditionInfo* prevState, Real prevAnimFraction);
	Real getCurrentAnimFraction() const;
	void applyCorrectModelStateAnimation();
	const ModelConditionInfo* findTransitionForSig(TransitionSig sig) const;
	void rebuildWeaponRecoilInfo(const ModelConditionInfo* state);
	void doHideShowProjectileObjects( UnsignedInt showCount, UnsignedInt maxCount, WeaponSlotType slot );///< Means effectively, show m of n.
	void nukeCurrentRender(Matrix3D* xform);
	void doStartOrStopParticleSys();
	void adjustAnimSpeedToMovementSpeed();
	static void hideAllMuzzleFlashes(const ModelConditionInfo* state, RenderObjClass* renderObject);
	void hideAllHeadlights(Bool hide);
#if defined(RTS_DEBUG)	//art wants to see buildings without flags as a test.
	void hideGarrisonFlags(Bool hide);
#endif
};
