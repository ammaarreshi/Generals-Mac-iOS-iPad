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

#pragma once

#include "always.h"
#include "rendobj.h"
#include "w3d_file.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "dx8wrapper.h"
#include "shader.h"
#include "vertmaterial.h"
#include "Lib/BaseType.h"
#include "Common/GameType.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"

#define MAX_ENABLED_DYNAMIC_LIGHTS 20
typedef UnsignedByte HeightSampleType;	//type of data to store in heightmap
class W3DTreeBuffer;
class W3DBibBuffer;
class W3DRoadBuffer;
class W3DBridgeBuffer;
class W3DWaypointBuffer;
class W3DTerrainLogic;
class W3DAssetManager;
class SimpleSceneClass;
class W3DShroud;
class W3DPropDrawModuleData;
class W3DPropBuffer;
class W3DTreeDrawModuleData;
class GeometryInfo;

#define no_TIMING_TESTS	1

#define no_PRE_TRANSFORM_VERTEX // Don't do this, not a performance win.  jba.

typedef struct {
	Int minX, maxX;
	Int minY, maxY;
} TBounds;


class LightMapTerrainTextureClass;
class CloudMapTerrainTextureClass;
class W3DDynamicLight;

#define DO_SCORCH 1

#define DO_ROADS 1

#ifdef DO_SCORCH
typedef struct {
	Vector3 location;
	Real		radius;
	Int			scorchType;
} TScorch;
#endif

#define VERTEX_FORMAT VertexFormatXYZDUV2
#define DX8_VERTEX_FORMAT DX8_FVF_XYZDUV2

/// Custom render object that draws the heightmap and handles intersection tests.
/**
Custom W3D render object that's used to process the terrain.  It handles
virtually everything to do with the terrain, including: drawing, lighting,
scorchmarks and intersection tests.
*/
class BaseHeightMapRenderObjClass : public RenderObjClass, public DX8_CleanupHook, public Snapshot
{

public:

	BaseHeightMapRenderObjClass();
	virtual ~BaseHeightMapRenderObjClass() override;

	// DX8_CleanupHook methods
	virtual void ReleaseResources() override;	///< Release all dx8 resources so the device can be reset.
	virtual void ReAcquireResources() override;  ///< Reacquire all resources after device reset.


	/////////////////////////////////////////////////////////////////////////////
	// Render Object Interface (W3D methods)
	/////////////////////////////////////////////////////////////////////////////
	virtual RenderObjClass *	Clone() const override;
	virtual int						Class_ID() const override;
	virtual void					Render(RenderInfoClass & rinfo) override = 0;
	virtual bool					Cast_Ray(RayCollisionTestClass & raytest) override; // This CANNOT be Bool, as it will not inherit properly if you make Bool == Int
	virtual void					Get_Obj_Space_Bounding_Sphere(SphereClass & sphere) const override;
	virtual void					Get_Obj_Space_Bounding_Box(AABoxClass & aabox) const override;


	virtual void					On_Frame_Update() override;
	virtual void					Notify_Added(SceneClass * scene) override;

  // Other VIRTUAL methods. [3/20/2003]

	///allocate resources needed to render heightmap
	virtual int initHeightData(Int width, Int height, WorldHeightMap *pMap, RefRenderObjListIterator *pLightsIterator, Bool updateExtraPassTiles=TRUE);
	virtual Int freeMapResources();	///< free resources used to render heightmap
	virtual void updateCenter(CameraClass *camera, const Vector3 *cameraPivot, RefRenderObjListIterator *pLightsIterator);
 	virtual void adjustTerrainLOD(Int adj);
	virtual void doPartialUpdate(const IRegion2D &partialRange, WorldHeightMap *htMap, RefRenderObjListIterator *pLightsIterator) = 0;
	virtual void staticLightingChanged();
	// GeneralsX @performance Mehmet Bat 01/09/2026 Expose deferred terrain dirty-region accumulation and consumption.
	void terrainHeightChanged(Int x, Int y);
	Bool takeDirtyRegion(IRegion2D &region);
	virtual void oversizeTerrain(Int tilesToOversize) = 0; ///< Oversize the visible terrain area.
	virtual void setTerrainDrawSize(Int width, Int height) = 0; ///< Resize the visible terrain area. Always defaults to oversize dimensions when oversize is set.
	virtual void reset();

  void redirectToHeightmap( WorldHeightMap *pMap )
  {
    REF_PTR_RELEASE( m_map );
	  m_hasDirtyRegion = false;
	  REF_PTR_SET(m_map, pMap);	//update our heightmap pointer in case it changed since last call.
  }


	UnsignedByte getClipHeight(Int x, Int y) const
	{
		Int xextent = m_map->getXExtent() - 1;
		Int yextent = m_map->getYExtent() - 1;

		if (x < 0)
			x = 0;
		else if (x > xextent)
			x = xextent;

		if (y < 0)
			y = 0;
		else if (y > yextent)
			y = yextent;

		return m_map->getDataPtr()[x + y*m_map->getXExtent()];
	}

	/// Update the macro texture (pass 3).
	void setTextureLOD(Int lod);	///<change the maximum mip-level sent to the hardware.
	void updateMacroTexture(AsciiString textureName);
	void doTextures(Bool flag) {m_disableTextures = !flag;};
	/// Update the diffuse value from static light info for one vertex.
	void doTheLight(VERTEX_FORMAT *vb, Vector3*light, Vector3*normal, RefRenderObjListIterator *pLightsIterator, UnsignedByte alpha);
	void addScorch(Vector3 location, Real radius, Scorches type);
	void addTree(DrawableID id, Coord3D location, Real scale, Real angle,
								Real randomScaleAmount,  const W3DTreeDrawModuleData *data);
	void removeAllTrees();
	void removeTree(DrawableID id);
	Bool updateTreePosition(DrawableID id, Coord3D location, Real angle);
	void renderTrees(CameraClass * camera); ///< renders the tree buffer.

	void addProp(Int id, Coord3D location, Real angle, Real scale, const AsciiString &modelName);
	void removeProp(Int id);
	void removeAllProps();

	void unitMoved( Object *unit );
	void notifyShroudChanged();
	void removeTreesAndPropsForConstruction(
		const Coord3D* pos,
		const GeometryInfo& geom,
		Real angle
	);

	/// Add a bib at location.
	void addTerrainBib(Vector3 corners[4], ObjectID id, Bool highlight);
	void addTerrainBibDrawable(Vector3 corners[4], DrawableID id, Bool highlight);
	/// Remove a bib.
	void removeTerrainBib(ObjectID id);
	void removeTerrainBibDrawable(DrawableID id);

	/// Removes all bibs.
	void removeAllTerrainBibs();
	/// Remove all highlighting.
	void removeTerrainBibHighlighting();

	W3DShroud *getShroud()	{return m_shroud;}
	void updateShorelineTiles(Int minX, Int minY, Int maxX, Int maxY, WorldHeightMap *pMap);	///<figure out which tiles on this map cross water plane
	void updateShorelineTile(Int X, Int Y, Int Border, WorldHeightMap *pMap);	///<figure out which tiles on this map cross water plane
	void recordShoreLineSortInfos();
	void updateViewImpassableAreas(Bool partial = FALSE, Int minX = 0, Int maxX = 0, Int minY = 0, Int maxY = 0);
	void clearAllScorches();
	void setTimeOfDay( TimeOfDay tod );
	void loadRoadsAndBridges(W3DTerrainLogic *pTerrainLogic, Bool saveGame); ///< Load the roads from the map objects.
	void worldBuilderUpdateBridgeTowers( W3DAssetManager *assetManager, SimpleSceneClass *scene );							///< for the editor updating of bridge tower visuals
	Int  getStaticDiffuse(Int x, Int y); ///< Gets the diffuse terrain lighting value for a point on the mesh.

	virtual Int	getNumExtraBlendTiles(Bool visible) { return 0;}
	Int getNumShoreLineTiles(Bool visible)	{ return visible?m_numVisibleShoreLineTiles:m_numShoreLineTiles;}
	void setShoreLineDetail();	///<update shoreline tiles in case the feature was toggled by user.
	Bool getMaximumVisibleBox(const FrustumClass &frustum,  AABoxClass *box, Bool ignoreMaxHeight);	///<3d extent of visible terrain.
	Real getHeightMapHeight(Real x, Real y, Coord3D* normal) const;	///<return height and normal at given point
	Bool isCliffCell(Real x, Real y);	///<return height and normal at given point
	Real getMinHeight() const {return m_minHeight;}	///<return minimum height of entire terrain
	Real getMaxHeight() const {return m_maxHeight;}	///<return maximum height of entire terrain
	Real getMaxCellHeight(Real x, Real y) const;	///< returns maximum height of the 4 cell corners.
	WorldHeightMap *getMap() {return m_map;}	///< returns object holding the heightmap samples - need this for fast access.
	Bool isClearLineOfSight(const Coord3D& pos, const Coord3D& posOther) const;

	Bool getShowImpassableAreas() {return m_showImpassableAreas;}
	void setShowImpassableAreas(Bool show) {m_showImpassableAreas = show;}

	Bool showAsVisibleCliff(Int xIndex, Int yIndex) const;

	Bool evaluateAsVisibleCliff(Int xIndex, Int yIndex, Real valuesGreaterThanRad);

	Real getViewImpassableAreaSlope() const { return m_curImpassableSlope; }
	void setViewImpassableAreaSlope(Real viewSlope) { m_curImpassableSlope = viewSlope; }

	Bool doesNeedFullUpdate() {return m_needFullUpdate;}


	virtual int updateBlock(Int x0, Int y0, Int x1, Int y1, WorldHeightMap *pMap, RefRenderObjListIterator *pLightsIterator) = 0;

protected:
	// snapshot methods
	virtual void crc( Xfer *xfer ) override;
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override;

protected:
	Int	m_x;	///< dimensions of heightmap
	Int	m_y;	///< dimensions of heightmap

#ifdef DO_SCORCH
	enum { MAX_SCORCH_VERTEX=8194,
					MAX_SCORCH_INDEX=6*8194,
					MAX_SCORCH_MARKS=500,
					SCORCH_MARKS_IN_TEXTURE=9,
					SCORCH_PER_ROW = 3};
	DX8VertexBufferClass	*m_vertexScorch;	///<Scorch vertex buffer.
	DX8IndexBufferClass			*m_indexScorch;	///<indices defining a triangles for the scorch drawing.
	TextureClass *m_scorchTexture;	///<Scorch mark texture
	Int			m_curNumScorchVertices;	 ///<number of vertices used in m_vertexScorch.
	Int			m_curNumScorchIndices;	 ///<number of indices used in m_indexScorch.
	TScorch	m_scorches[MAX_SCORCH_MARKS];
	Int			m_numScorches;

	Int			m_scorchesInBuffer;		///< how many are in the buffers.  If less than numScorches, we need to update

	// NOTE: This argument (contrary to most of the rest of the engine), is in degrees, not radians.
	Real		m_curImpassableSlope;

	void updateScorches();	 ///<Update m_vertexScorch and m_indexScorch so all scorches will be drawn.
	void allocateScorchBuffers();	 ///<allocate static buffers for drawing scorch marks.
	void freeScorchBuffers();		 ///< frees up scorch buffers.
	void drawScorches();		///< Draws the scorch mark polygons in m_vertexScorch.
#endif
	WorldHeightMap *m_map;
	Bool m_useDepthFade;	///<fade terrain lighting under water
	Bool m_updating;
	Vector3 m_depthFade;	///<depth based fall off values for r,g,b
	Bool m_disableTextures;
	Bool m_needFullUpdate; ///< True if lighting changed, and we need to update all instead of what moved.
	// GeneralsX @performance Mehmet Bat 01/09/2026 Track a bounded terrain region until its consumer updates it.
	IRegion2D m_dirtyRegion;
	Bool m_hasDirtyRegion;
	Bool m_doXNextTime; ///< True if we updated y scroll, and need to do x scroll next frame.
	Real	m_minHeight;	///<minimum value of height samples in heightmap
	Real	m_maxHeight;	///<maximum value of height samples in heightmap
	Bool m_showImpassableAreas; ///< If true, shade impassable areas.

	// STL is "smart." This is a variable sized bitset. Very memory efficient.
	std::vector<bool> m_showAsVisibleCliff;


	ShaderClass m_shaderClass; ///<shader or rendering state for heightmap
	VertexMaterialClass	  	  *m_vertexMaterialClass;	///< vertex shader (lighting) for terrain
	TextureClass *m_stageZeroTexture;	///<primary texture
	TextureClass *m_stageOneTexture;	///<transparent edging texture
	CloudMapTerrainTextureClass *m_stageTwoTexture;	///<Cloud map texture
	TextureClass *m_stageThreeTexture;	///<light/noise map texture
	AsciiString m_macroTextureName; ///< Name for stage 3 texture.
	TextureClass *m_destAlphaTexture;	///< Texture holding destination alpha LUT for water depth.

	W3DTreeBuffer *m_treeBuffer; ///< Class for drawing trees and other alpha objects.
	W3DPropBuffer *m_propBuffer; ///< Class for drawing trees and other alpha objects.
	W3DBibBuffer *m_bibBuffer; ///< Class for drawing trees and other alpha objects.
	W3DWaypointBuffer *m_waypointBuffer; ///< Draws waypoints.
#ifdef DO_ROADS
	W3DRoadBuffer *m_roadBuffer; ///< Class for drawing roads.
#endif
	W3DBridgeBuffer *m_bridgeBuffer;
	W3DShroud *m_shroud;	///< Class for drawing the shroud over terrain.
	struct shoreLineTileInfo
	{	Int m_xy;	//x,y position of tile
		Real verts[4*3];	//position of tile vertices. 4 verts with 3 components.
		Real t0,t1,t2,t3;	//index into water depth alpha LUT.
	};

	//at each x or y coordinate of the map (depending on major axis) we store a list of
	//shoreline blend tiles having the same x or y coordinate.  This makes searching the
	//shoreline tiles much faster.
	struct shoreLineTileSortInfo
	{
		Int	tileStartIndex;	//index within m_shoreLineTilePositions where tiles start
		Int numTiles;		//total tiles at this coordinate.
		UnsignedShort minTileCoordinate;	//lowest coordinate in list
		UnsignedShort maxTileCoordinate;	//highest coordinate in list.
	};

	shoreLineTileInfo *m_shoreLineTilePositions;	///<array holding x,y tile positions of all shoreline terrain.
	Int m_numShoreLineTiles;		///<number of tiles in m_shoreLineTilePositions.
	Int m_numVisibleShoreLineTiles;	///<number of visible tiles.
	Int m_shoreLineTilePositionsSize;	///<total size of array including unused memory.
	Real m_currentMinWaterOpacity;		///<current value inside the gradient lookup texture.
	shoreLineTileSortInfo *m_shoreLineSortInfos;
	Int m_shoreLineSortInfosSize;
	Int m_shoreLineSortInfosXMajor;
	Int m_shoreLineTileSortMaxCoordinate;	///<keep track of coordinate range along axis used for m_shoreLineSortInfos
	Int m_shoreLineTileSortMinCoordinate;
	void initDestAlphaLUT();	///<initialize water depth LUT stored in m_destAlphaTexture
	void renderShoreLines(CameraClass *pCamera);	///<re-render parts of terrain that need custom blending into water edge
	void renderShoreLinesSorted(CameraClass *pCamera);	///<optimized version for game usage.

	static Bool useCloud();
};

extern BaseHeightMapRenderObjClass *TheTerrainRenderObject;
