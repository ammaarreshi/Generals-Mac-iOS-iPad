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

// FILE: W3DVolumetricShadow.cpp ///////////////////////////////////////////////////////////
//
// Real time shadow volume representations
//
// Author: Colin Day, January 2001
// Adapted for W3D: Mark Wilczynski October 2001
//
//
///////////////////////////////////////////////////////////////////////////////
///@todo: Must cap shadow volumes if we ever allow camera inside the volumes.
///@todo: Find better way to determine when shadow volumes need updating - lights move, objects move.

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
#include <assert.h>

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "always.h"
#include "GameClient/View.h"
#include "WW3D2/camera.h"
#include "WW3D2/light.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/hlod.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "Lib/BaseType.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "d3dx8math.h"
#include "Common/GlobalData.h"
#include "Common/DrawModule.h"
#include "W3DDevice/GameClient/W3DVolumetricShadow.h"
#include "W3DDevice/GameClient/W3DShadow.h"
#include "WW3D2/statistics.h"
#include "GameLogic/TerrainLogic.h"
#include "WW3D2/dx8caps.h"
#include "GameClient/Drawable.h"
#ifdef USE_WWSHADE
#include "wwshade/shdmesh.h"
#include "wwshade/shdsubmesh.h"
#endif


// Global Variables and Functions /////////////////////////////////////////////

W3DVolumetricShadowManager	*TheW3DVolumetricShadowManager=nullptr;
extern const FrustumClass *shadowCameraFrustum;	//defined in W3DShadow.

///////////////////////////////////////////////////////////////////////////////
// DEFINITIONS ////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// when the change in angle from the object to the light source
// (represented in degrees in the first number below)
// is large enough the shadow info will be reconstructed
const Real cosAngleToCare = cos ((0.2 * PI) / 180.0);	//1.5 degree difference
#define MAX_SILHOUETTE_EDGES	1024	//maximum number of shadov volume sides or edges in silhoutte
#define	SHADOW_EXTRUSION_BUFFER	0.1f		//amount to extend shadow volume beyond what's required to hit ground.
#define AIRBORNE_UNIT_GROUND_DELTA 2.0f
#define MAX_SHADOW_LENGTH_SCALE_FACTOR	1.0f //amount shadow can extend beyond the objects normal bounding sphere
#define MAX_SHADOW_LENGTH_EXTRA_AIRBORNE_SCALE_FACTOR 1.5f	//scales MAX_SHADOW_LENGTH_SCALE_FACTOR a little more for flying units
#define MAX_EXTRUSION_LENGTH (512.0f*MAP_XY_FACTOR)	//maximum length of a shadow extrusion - assumed as 512x512 cell map for now.
#define MAX_SHADOW_EXTRUSION_UNDER_OBJECT_BEFORE_CLAMP	5.0f		//maximum amount that shadow can reach below object base (z-position) before we clamp it's length to reduce artifacts.
#define SHADOW_SAMPLING_INTERVAL (MAP_XY_FACTOR * 2.0f)				//stepsize along ray used to find lowest point on terrain within shadow's reach.
#define OVERHANGING_OBJECT_CLAMP_ANGLE	(80.0f/180.0f*PI)				//for objects that are right on a cliff edge, clamp light angle to cast a nearly vertical shadow.

//#define SV_DEBUG
//#define SV_DEBUG_BOUNDS

struct SHADOW_STATIC_VOLUME_VERTEX	//vertex structure passed to D3D
{
		float x,y,z;
};
#define SHADOW_STATIC_VOLUME_FVF	D3DFVF_XYZ

#ifdef SV_DEBUG	//in debug mode, dynamic shadows are rendered with random diffuse color
	struct SHADOW_DYNAMIC_VOLUME_VERTEX	//vertex structure passed to D3D
	{
			float x,y,z;
			DWORD diffuse;
	};
	#define SHADOW_DYNAMIC_VOLUME_FVF	D3DFVF_XYZ|D3DFVF_DIFFUSE
#else
	typedef struct SHADOW_STATIC_VOLUME_VERTEX	SHADOW_DYNAMIC_VOLUME_VERTEX;
	#define SHADOW_DYNAMIC_VOLUME_FVF	D3DFVF_XYZ
#endif

LPDIRECT3DVERTEXBUFFER8 shadowVertexBufferD3D=nullptr;		///<D3D vertex buffer
LPDIRECT3DINDEXBUFFER8	shadowIndexBufferD3D=nullptr;	///<D3D index buffer
int nShadowVertsInBuf=0;	//model vetices in vertex buffer
int nShadowStartBatchVertex=0;
int nShadowIndicesInBuf=0;	//model vetices in vertex buffer
int nShadowStartBatchIndex=0;
int SHADOW_VERTEX_SIZE=4096;
int SHADOW_INDEX_SIZE=8192;

//Rough bounding box around visible portion of the terrain
//useful for quick culling
static Real bcX;
static Real bcY;
static Real bcZ;
static Real beX;
static Real beY;
static Real beZ;

static LPDIRECT3DVERTEXBUFFER8 lastActiveVertexBuffer=nullptr;

/** A simple structure to hold random geometry (vertices, polygons, etc.).  We'll use this
* to store shadow volumes. */
struct Geometry
{
	enum VisibleState {
		STATE_UNKNOWN = CollisionMath::BOTH,
		STATE_VISIBLE = CollisionMath::INSIDE,
		STATE_INVISIBLE = CollisionMath::OUTSIDE,
	};

	Geometry() : m_verts(nullptr),m_indices(nullptr),m_numPolygon(0),m_numVertex(0),m_flags(0) {}
	~Geometry() { Release();}

	Int Create( Int numVertices, Int numPolygons )
	{
		if (numVertices)
			if((m_verts=NEW Vector3[numVertices]) == nullptr)
				return FALSE;
		if (numPolygons)
			if((m_indices=NEW UnsignedShort[numPolygons*3]) == nullptr)
				return FALSE;
		m_numPolygon=numPolygons;
		m_numVertex=numVertices;
		m_numActivePolygon=0;
		m_numActiveVertex=0;
		return TRUE;
	}
	void Release()
	{
		delete [] m_verts;
		m_verts=nullptr;

		delete [] m_indices;
		m_indices=nullptr;

		m_numActivePolygon=m_numPolygon=0;
		m_numActiveVertex=m_numVertex=0;
	}
	Int GetFlags () { return m_flags;}
	void SetFlags (Int flags) { m_flags = flags;}
	Int GetNumPolygon () { return m_numPolygon;}
	Int GetNumVertex ()	{ return m_numVertex;}
	Int GetNumActivePolygon () { return m_numActivePolygon;}
	Int GetNumActiveVertex ()	{ return m_numActiveVertex;}
	Int SetNumActivePolygon (Int numPolygons) { return m_numActivePolygon=numPolygons;}
	Int SetNumActiveVertex (Int numVertices)	{ return m_numActiveVertex=numVertices;}
	UnsignedShort *GetPolygonIndex (long dwPolyId, short *psIndexList) const
	{
		*psIndexList++ = m_indices[dwPolyId*3];
		*psIndexList++ = m_indices[dwPolyId*3+1];
		*psIndexList++ = m_indices[dwPolyId*3+2];
		return &m_indices[dwPolyId];
	}
	Int SetPolygonIndex (long dwPolyId, short *psIndexList)
	{
		m_indices[dwPolyId*3]=psIndexList[0];
		m_indices[dwPolyId*3+1]=psIndexList[1];
		m_indices[dwPolyId*3+2]=psIndexList[2];
		return 3;
	}
	Vector3 *GetVertex (int dwVertId)
	{
		return &m_verts[dwVertId];
	}
	const Vector3 *SetVertex (int dwVertId, const Vector3 *pvVertex)
	{
		m_verts[dwVertId]=*pvVertex;
		return 	pvVertex;
	}
	///Find a vertex within given range
	Int	FindVertexInRange (Int start, Int end, Vector3 *pvVertex)
	{
		for (Int i=start; i<end; i++)
		{
			if ((m_verts[i]-*pvVertex).Length2() == 0)
				return i;
		}
		return -1;
	}

	AABoxClass &getBoundingBox() {return m_boundingBox;}
	void	setBoundingBox(const AABoxClass &box)	{m_boundingBox=box;}
	void	setBoundingSphere(const SphereClass &sphere) {m_boundingSphere=sphere;}
	SphereClass &getBoundingSphere() {return m_boundingSphere;}
	void	setVisibleState(VisibleState state)	{m_visibleState=state;}
	VisibleState	getVisibleState() {return m_visibleState;}

private:
	Vector3	*m_verts;
	UnsignedShort *m_indices;
	Int m_numPolygon;
	Int m_numVertex;
	Int m_numActivePolygon;	///<number of polygons filled with valid data
	Int m_numActiveVertex;		///<number of vertices filled with valid data
	Int	m_flags;				///<geometry attribute flags - static vs. dynamic, etc.
	AABoxClass m_boundingBox;	///<object space bounding box of shadow volume
	SphereClass m_boundingSphere;	///<object space bounding sphere of shadow volume
	VisibleState	m_visibleState;		///<flag if this geometry was visible in this frame.
};

// CONST //////////////////////////////////////////////////////////////////////
const Int MAX_POLYGON_NEIGHBORS = 3;  // we use nothing but triangles for
																			// geometry polygons so we have at
																			// most 3 neighbors
const Int NO_NEIGHBOR = -1;  // entry value for neighbor when there isn't one

const Byte POLY_VISIBLE	  = 0x01;  // polygon is visible from light
const Byte POLY_PROCESSED = 0x02;  // this poly has been processed

// STRUCT /////////////////////////////////////////////////////////////////////

// NeighborEdge ---------------------------------------------------------------
typedef struct _NeighborEdge
{

	Short neighborIndex;  // index of polygon who is our neighbor, if there is
												// not a neighbor it contains NO_NEIGHBOR
	Short neighborEdgeIndex[ 2 ];  // the two vertex indices that represent the
																 // shared edge

} NeighborEdge;

// PolygonNeighbor ------------------------------------------------------------
struct  PolyNeighbor
{

	Short myIndex;  // our polygon index so we know who we are
	Byte status;  // status flags used when processing neighbors
	NeighborEdge neighbor[ MAX_POLYGON_NEIGHBORS ];

};

/**This class holds original mesh specific data and geometry.  The meshes stored in this
class have been cleaned to remove replicated vertices and also cache mesh data needed for
faster silhouette computation.  A model can contain many meshes for which we need to store
separate data so they can move relative to each other.*/
class W3DShadowGeometryMesh
{
	//for the sake of speed, give direct access to classes that need this data.
	friend class W3DShadowGeometry;
	friend class W3DVolumetricShadow;

public:
	W3DShadowGeometryMesh();
#ifdef DO_TERRAIN_SHADOW_VOLUMES
	virtual
#endif
	~W3DShadowGeometryMesh();

	/// @todo: Cache/Store face normals someplace so they are not recomputed when lights move.
	const Vector3& GetPolygonNormal(long dwPolyNormId) const
	{
		WWASSERT(m_polygonNormals);
		return m_polygonNormals[dwPolyNormId];
	}
	int GetNumPolygon () const {return m_numPolygons;}
	/// given loaded geometry this builds the polygon neighbor information
	void buildPolygonNeighbors();
	void buildPolygonNormals()
	{
		if (!m_polygonNormals)
		{	//need to allocate storage
			Vector3 *tempVec = NEW Vector3[m_numPolygons];
			for (int i=0; i<m_numPolygons; i++)
			{
				buildPolygonNormal(i,&tempVec[i]);
			}
			m_polygonNormals = tempVec;
		}
	}
protected:
	Vector3 *buildPolygonNormal (long dwPolyNormId, Vector3 *pvNorm) const
	{
		if (m_polygonNormals)
			return &(*pvNorm=m_polygonNormals[dwPolyNormId]);
		short indexList[3];
//		Vector3 vertexList[3];
		//get vertex indices for this polygon
		GetPolygonIndex(dwPolyNormId,indexList);
		//get the vertices
//		GetVertex(indexList[0],&vertexList[0]);
//		GetVertex(indexList[1],&vertexList[1]);
//		GetVertex(indexList[2],&vertexList[2]);
		const Vector3& v0=GetVertex(indexList[0]);
		const Vector3& v1=GetVertex(indexList[1]);
		const Vector3& v2=GetVertex(indexList[2]);

		//compute triangle normal by crossing 2 edges
		Vector3 edge1=v1-v0;
		Vector3 edge2=v1-v2;
#ifdef ALLOW_TEMPORARIES
		*pvNorm=Vector3::Cross_Product(edge2,edge1);
		pvNorm->Normalize();
#else
		Vector3::Normalized_Cross_Product(edge2,edge1, pvNorm);
#endif
		return pvNorm;
	}

	/// creating and deleting storage for the polygon neighbors
	Bool allocateNeighbors( Int numPolys );
	void deleteNeighbors();

	// geometry shadow data access
	PolyNeighbor *GetPolyNeighbor( Int polyIndex );
	int GetNumVertex ()	const {	return m_numVerts;}
	///Get indices to the 3 vertices of this face.
#ifdef DO_TERRAIN_SHADOW_VOLUMES
	virtual
#endif
	void GetPolygonIndex (long dwPolyId, short *psIndexList) const
	{	const TriIndex *polyi=&m_polygons[dwPolyId];
		*psIndexList++ = m_parentVerts[polyi->I];
		*psIndexList++ = m_parentVerts[polyi->J];
		*psIndexList++ = m_parentVerts[polyi->K];
	}
#ifdef DO_TERRAIN_SHADOW_VOLUMES
	virtual
#endif
	const Vector3& GetVertex (int dwVertId) const
	{
		return m_verts[dwVertId];
	}

	MeshClass *m_mesh;	///< W3D mesh for this geometry
	Int m_meshRobjIndex;	///<index of this mesh within hlod robj
	const Vector3	*m_verts;		///<array of vertices
	Vector3	*m_polygonNormals;	///<array of face normals
	Int m_numVerts;	 ///< number of actual vertices after duplicates are removed.
	Int m_numPolygons; ///<number of polygons in source geometry
	const TriIndex	*m_polygons;	///<array of 3 vertex indices per face
	UnsignedShort *m_parentVerts;	///<array of parent vertex indices for each vertex.
	/// the neighbor info indexed by polygon id
	PolyNeighbor *m_polyNeighbors;
	Int m_numPolyNeighbors;  // length of m_polyNeighbors and the number of polygons
							 // in our current geometry.
	W3DShadowGeometry *m_parentGeometry; // mesh hierarchy containing this mesh.

};

#ifdef DO_TERRAIN_SHADOW_VOLUMES

//Custom version of W3DShadowGeometryMesh for meshes stored as heightmap
class W3DShadowGeometryHeightmapMesh : public W3DShadowGeometryMesh
{

public:
	virtual int GetPolygonIndex (long dwPolyId, short *psIndexList) const;
	virtual Vector3 *GetVertex (int dwVertId, Vector3 *pvVertex);
	W3DShadowGeometryHeightmapMesh() : m_patchOriginX(0),m_patchOriginY(0) { }
	void setPatchOrigin(Int x, Int y) {m_patchOriginX=x; m_patchOriginY=y;}
	void getPatchOrigin(Int *x, Int *y) {*x=m_patchOriginX; *y=m_patchOriginY;}
	void setPatchSize(Int size)	{m_width=size; m_numPolygons=(size-1)*(size-1)*2;}
	Int getPatchSize()	{return m_width;}

	protected:

		Int m_heightmapPitch;	///<width of full heightmap of which this mesh is a sub-rectangle
		Int m_width;			///<patch width
		Int m_patchOriginX;		///<location of patch within parent heightmap
		Int m_patchOriginY;		///<location of patch within parent heightmap
};

int W3DShadowGeometryHeightmapMesh::GetPolygonIndex (long dwPolyId, short *psIndexList) const
{
	//Find top left vertex of cell containing polygon
	WorldHeightMap *map=nullptr;
	if (TheTerrainRenderObject)
		map=TheTerrainRenderObject->getMap();
	if (!map)
		return 0;

	Int row=dwPolyId/((m_width-1)<<1);
	Int column=(dwPolyId>>1)-row*((m_width-1));

#ifdef FLIP_TRIANGLES
	UnsignedByte alpha[4];
	float UA[4], VA[4];
	Bool flipForBlend;
	map->getAlphaUVData(column+m_patchOriginX, row+m_patchOriginY, UA, VA, alpha, &flipForBlend);
	if (flipForBlend)
	{
		if (dwPolyId &1)
		{	psIndexList[0]=row*m_width+column+1;
			psIndexList[1]=(row+1)*m_width+column+1;
			psIndexList[2]=(row+1)*m_width+column;
		}
		else
		{	psIndexList[0]=row*m_width+column;
			psIndexList[1]=row*m_width+column+1;
			psIndexList[2]=(row+1)*m_width+column;
		}
	}
	else
#endif
	{	if (dwPolyId &1)
		{	psIndexList[0]=row*m_width+column;
			psIndexList[1]=row*m_width+column+1;
			psIndexList[2]=(row+1)*m_width+column+1;
		}
		else
		{	psIndexList[0]=row*m_width+column;
			psIndexList[1]=(row+1)*m_width+column+1;
			psIndexList[2]=(row+1)*m_width+column;
		}
	}

	return 3;
}

Vector3 *W3DShadowGeometryHeightmapMesh::GetVertex (int dwVertId, Vector3 *pvVertex)
{
	WorldHeightMap *map=nullptr;

	if (TheTerrainRenderObject)
		map=TheTerrainRenderObject->getMap();

	if (!map)
		return nullptr;

	Int row=dwVertId/m_width;
	Int column=dwVertId-row*m_width;

	UnsignedByte *data=map->getDataPtr();
	pvVertex->X=(m_patchOriginX+column)*MAP_XY_FACTOR;
	pvVertex->Y=(m_patchOriginY+row)*MAP_XY_FACTOR;
	pvVertex->Z=(Real)data[(m_patchOriginX+column)+(m_patchOriginY+row)*map->getXExtent()]*MAP_HEIGHT_SCALE;

	return pvVertex;
}

Bool isPatchShadowed(W3DShadowGeometryHeightmapMesh	*hm_mesh)
{
	WorldHeightMap *map=nullptr;
	Short poly[ 3 ];
	Vector3 vertex;
	Vector3 normal,lightVector;
	Int firstVisible=0;
	Int testVisible;

	if (TheTerrainRenderObject)
		map=TheTerrainRenderObject->getMap();

	if (!map)
		return FALSE;

	hm_mesh->buildPolygonNormal( 0, &normal );

	// get the vertex indices at this polygon
	hm_mesh->GetPolygonIndex( 0, poly, 3 );

	//
	// find out "lightVector" to this polygon
	//
	// since our light source could be very close to the object and that
	// would change the shadow we are going to say that the light vector
	// is from the light position to one of the vertices in the polygon.
	// To be more correct we should use the center of the polygon but
	// this is a good approximation ... an ever broader approximation that
	// we could use would be the object center
	//
	hm_mesh->GetVertex( poly[ 0 ], &vertex );
	lightVector= vertex - LightPosWorld[0];

	//
	// dot the light vector with the normal of the polygon to see if the
	// poly is visible from this location
	//
	if( Vector3::Dot_Product( lightVector, normal ) < 0.0f )
		firstVisible=1;

	for (Int i=1; i<hm_mesh->GetNumPolygon(); i++)
	{
		hm_mesh->buildPolygonNormal( i, &normal );

		// get the vertex indices at this polygon
		hm_mesh->GetPolygonIndex( i, poly, 3 );

		//
		// find out "lightVector" to this polygon
		//
		// since our light source could be very close to the object and that
		// would change the shadow we are going to say that the light vector
		// is from the light position to one of the vertices in the polygon.
		// To be more correct we should use the center of the polygon but
		// this is a good approximation ... an ever broader approximation that
		// we could use would be the object center
		//
		hm_mesh->GetVertex( poly[ 0 ], &vertex );
		lightVector= vertex - LightPosWorld[0];

		//
		// dot the light vector with the normal of the polygon to see if the
		// poly is visible from this location
		//
		testVisible=0;
		if( Vector3::Dot_Product( lightVector, normal ) < 0.0f )
			testVisible=1;

//		if (testVisible ^ firstVisible)
//			return TRUE;	//found polys facing different directions to sun, will cast shadow
		if (!testVisible)
			return TRUE;	//some part of mesh not facing light, so it could cast a shadow
	}
	return FALSE;
}

#define SV_MAX_TERRAIN_MESHES	16

static W3DShadowGeometryHeightmapMesh terrainMeshes[SV_MAX_TERRAIN_MESHES];
static Int numTerrainMeshes=0;

void W3DVolumetricShadowManager::loadTerrainShadows()
{
	WorldHeightMap *map=nullptr;
	Int patchSize=3;

	if (TheTerrainRenderObject)
		map=TheTerrainRenderObject->getMap();

	if (!map)
		return;

	for (Int y=0; y<map->getYExtent(); y += patchSize-1)
	{
		for (Int x=0; x<map->getXExtent(); x += patchSize-1)
		{
			W3DShadowGeometryHeightmapMesh	*hm_mesh=&terrainMeshes[numTerrainMeshes];
			hm_mesh->setPatchOrigin(x,y);
			hm_mesh->setPatchSize(patchSize);

			if(isPatchShadowed(hm_mesh))
			{	//some polygons in this patch cast shadows, need to generate a mesh
//				hm_mesh->buildPolygonNeighbors();
				numTerrainMeshes++;
			}
		}
	}

/*	W3DShadowGeometryHeightmapMesh	hm_mesh;
	short indexList[3];
	Vector3 vertexList[3];
*/
/*	W3DShadow *shadow = NEW W3DShadow;
	// add to our shadow list through the shadow next links
	shadow->m_next = m_shadowList;
	m_shadowList = shadow;
*/
}

#endif //DO_TERRAIN_SHADOW_VOLUMES

/** This class will wrap any shadow casting geometry with additional
data needed for efficient shadow volume generation.  The W3DVolumetricShadowManager
will allocate these structures and hash them for quick re-use on other
models sharing the same geometry.*/
class W3DShadowGeometry : public RefCountClass, public	HashableClass
{

	public:

		W3DShadowGeometry() { };
		virtual ~W3DShadowGeometry() override { };

		virtual	const char * Get_Key() override { return m_namebuf;	}

		Int init (RenderObjClass *robj);
		Int initFromHLOD (RenderObjClass *robj);	///<initialize the geometry from a W3D HLOD object.
		Int initFromMesh (RenderObjClass *robj);///<initialize the geometry from a W3D Mesh object.

		const char *		Get_Name() const	{ return m_namebuf;}
		void				Set_Name(const char *name)
		{
			strlcpy(m_namebuf,name,sizeof(m_namebuf));
		}
		Int					getMeshCount()	{ return m_meshCount;}
		W3DShadowGeometryMesh	*getMesh(Int index)	{ return &m_meshList[index];}


		int GetNumTotalVertex ()	{	return m_numTotalsVerts;}	///<total number of vertices in all meshes of this geometry

	private:

		char m_namebuf[2*W3D_NAME_LEN];	///<name of model hierarchy

		W3DShadowGeometryMesh m_meshList[MAX_SHADOW_CASTER_MESHES]; ///<collection of meshes for this geometry.
		Int m_meshCount;							///<number of meshes in hierarchy
		Int m_numTotalsVerts;						///<number of verts in entire hierarchy
};

#define MAX_SHADOW_VOLUME_VERTS 16384

Int W3DShadowGeometry::initFromHLOD(RenderObjClass *robj)
{
	HLodClass *hlod=(HLodClass *)robj;
	//locations of parent vertices inside the vertex array after duplicate
	//vertices are removed.
	UnsignedShort vertParent[MAX_SHADOW_VOLUME_VERTS];

	Int i,j,k,newVertexCount;

	Int top = hlod->Get_LOD_Count()-1;
	W3DShadowGeometryMesh *geomMesh=&m_meshList[m_meshCount];

	m_numTotalsVerts=0;

	for (i = 0; i < hlod->Get_Lod_Model_Count(top); i++)
	{
		if (hlod->Peek_Lod_Model(top,i) && hlod->Peek_Lod_Model(top,i)->Class_ID() == RenderObjClass::CLASSID_MESH)
		{
			DEBUG_ASSERTCRASH(m_meshCount < MAX_SHADOW_CASTER_MESHES, ("Too many shadow sub-meshes"));

			geomMesh->m_mesh = (MeshClass *)hlod->Peek_Lod_Model(top,i);
			geomMesh->m_meshRobjIndex=i;

//			if (!geomMesh->m_mesh->Peek_Model()->Get_Flag(MeshGeometryClass::CAST_SHADOW))
//				continue; // CNC3 (gth) Only cast shadows from meshes with the shadow flag ENABLED!

			if ((geomMesh->m_mesh->Is_Alpha() || geomMesh->m_mesh->Is_Translucent()) && !geomMesh->m_mesh->Peek_Model()->Get_Flag(MeshGeometryClass::CAST_SHADOW))
				continue; //transparent meshes that don't have forced shadows will not cast volumetric shadows
			// CNC3 (gth) skin meshes should never cast a volumetric shadow
			if (geomMesh->m_mesh->Peek_Model()->Get_Flag(MeshGeometryClass::SKIN))
				continue;

			MeshModelClass *mm = geomMesh->m_mesh->Peek_Model();
			geomMesh->m_numVerts=mm->Get_Vertex_Count();
			geomMesh->m_verts=mm->Get_Vertex_Array();
			geomMesh->m_numPolygons=mm->Get_Polygon_Count();
			geomMesh->m_polygons=mm->Get_Polygon_Array();

			if (geomMesh->m_numVerts > MAX_SHADOW_VOLUME_VERTS)
				return FALSE;	//too many vertices to process

			//reset index of all vertices
			memset(vertParent,0xffffffff,sizeof(vertParent));
			newVertexCount=geomMesh->m_numVerts;
			//Find all duplicated vertices.
			for (j=0; j<geomMesh->m_numVerts; j++)
			{
				if (vertParent[j] != 0xffff)
					continue;	//this vertex has already been processed

				const Vector3 *v_curr=&geomMesh->m_verts[j];

				for (k=j+1; k<geomMesh->m_numVerts; k++)
				{
					Vector3 len(*v_curr - geomMesh->m_verts[k]);
					if (len.Length2() == 0)
					{	//found duplicate vertex
						vertParent[k]=j;
						newVertexCount--;	//decrease total vertices since duplicate found.
					}
				}
				vertParent[j]=j;	//first instance of new vertex
			}
			geomMesh->m_parentVerts = NEW UnsignedShort[geomMesh->m_numVerts];
			memcpy(geomMesh->m_parentVerts,vertParent,sizeof(UnsignedShort)*geomMesh->m_numVerts);
			geomMesh->m_numVerts=newVertexCount;	//adjust actual vertex count to ignore duplicates
			m_numTotalsVerts += newVertexCount;
			geomMesh->m_parentGeometry = this;

			// build our neighboring polygon information
//			geomMesh->buildPolygonNeighbors();

			geomMesh++;
			m_meshCount++;
		}


#ifdef USE_WWSHADE //(cnc3)(gth) Support for ShaderMeshes!
// I'm coding this as a completely independent block rather than re-factoring the code above
// because it will probably save us pain in future merges.
		if (hlod->Peek_Lod_Model(top,i) && hlod->Peek_Lod_Model(top,i)->Class_ID() == RenderObjClass::CLASSID_SHDMESH)
		{
			DEBUG_ASSERTCRASH(m_meshCount < MAX_SHADOW_CASTER_MESHES, ("Too many shadow sub-meshes"));

			ShdMeshClass * shd_mesh = (ShdMeshClass *)hlod->Peek_Lod_Model(top,i);

			for (int sub_mesh_index=0; sub_mesh_index < shd_mesh->Get_Sub_Mesh_Count(); sub_mesh_index++) {
				ShdSubMeshClass * sub_mesh = shd_mesh->Peek_Sub_Mesh(sub_mesh_index);

				if (!sub_mesh->Get_Flag(MeshGeometryClass::CAST_SHADOW))
					continue; // CNC3 (gth) Only cast shadows from meshes with the shadow flag ENABLED!

				//transparent meshes that don't have forced shadows will not cast volumetric shadows
				if (shd_mesh->Is_Translucent() && !sub_mesh->Get_Flag(MeshGeometryClass::CAST_SHADOW))
					continue;

				// skin meshes should never cast a volumetric shadow
				if (sub_mesh->Get_Flag(MeshGeometryClass::SKIN))
					continue;

				geomMesh->m_mesh = nullptr; //hope this doesn't cause problems!
				geomMesh->m_meshRobjIndex=i;

				// Count the polygons and vertices
				geomMesh->m_numVerts = sub_mesh->Get_Vertex_Count();
				geomMesh->m_numPolygons = sub_mesh->Get_Polygon_Count();

				geomMesh->m_verts=sub_mesh->Get_Vertex_Array();
				geomMesh->m_polygons=sub_mesh->Get_Polygon_Array();

				if (geomMesh->m_numVerts > MAX_SHADOW_VOLUME_VERTS)
					return FALSE;	//too many vertices to process

				//reset index of all vertices
				memset(vertParent,0xffffffff,sizeof(vertParent));
				newVertexCount=geomMesh->m_numVerts;
				//Find all duplicated vertices.
				for (j=0; j<geomMesh->m_numVerts; j++)
				{
					if (vertParent[j] != 0xffff)
						continue;	//this vertex has already been processed

					const Vector3 *v_curr=&geomMesh->m_verts[j];

					for (k=j+1; k<geomMesh->m_numVerts; k++)
					{
						Vector3 len(*v_curr - geomMesh->m_verts[k]);
						if (len.Length2() == 0)
						{	//found duplicate vertex
							vertParent[k]=j;
							newVertexCount--;	//decrease total vertices since duplicate found.
						}
					}
					vertParent[j]=j;	//first instance of new vertex
				}
				geomMesh->m_parentVerts = new UnsignedShort[geomMesh->m_numVerts];
				memcpy(geomMesh->m_parentVerts,vertParent,sizeof(UnsignedShort)*geomMesh->m_numVerts);
				geomMesh->m_numVerts=newVertexCount;	//adjust actual vertex count to ignore duplicates
				m_numTotalsVerts += newVertexCount;
				geomMesh->m_parentGeometry = this;

				// build our neighboring polygon information
//				geomMesh->buildPolygonNeighbors();

				geomMesh++;
				m_meshCount++;

			}
		}
#endif //(cnc3)(gth) Support for ShaderMeshes!

	}

//	for (i = 0; i < AdditionalModels.Count(); i++) {
//		res |= AdditionalModels[i].Model->Cast_Ray(raytest);
//	}

	return m_meshCount != 0;
}

Int W3DShadowGeometry::initFromMesh(RenderObjClass *robj)
{
	//locations of parent vertices inside the vertex array after duplicate
	//vertices are removed.
	UnsignedShort vertParent[MAX_SHADOW_VOLUME_VERTS];

	Int j,k,newVertexCount;
	W3DShadowGeometryMesh *geomMesh=&m_meshList[m_meshCount];

	assert (m_meshCount < MAX_SHADOW_CASTER_MESHES);

	geomMesh->m_mesh = (MeshClass *)robj;
	geomMesh->m_meshRobjIndex = -1;	//robj is the mesh so no index needed.
	if (((geomMesh->m_mesh->Is_Alpha() || geomMesh->m_mesh->Is_Translucent()) && !geomMesh->m_mesh->Peek_Model()->Get_Flag(MeshGeometryClass::CAST_SHADOW)))
		return FALSE; //transparent meshes that don't have forced shadows will not cast volumetric shadows

	// GeneralsX @bugfix Mr. Meeseeks 17/06/2026 Skin meshes should never cast a volumetric shadow (matches initFromHLOD behavior and Zero Hour upstream)
	if (geomMesh->m_mesh->Peek_Model()->Get_Flag(MeshGeometryClass::SKIN))
		return FALSE;

	MeshModelClass *mm = geomMesh->m_mesh->Peek_Model();
	geomMesh->m_numVerts=mm->Get_Vertex_Count();
	geomMesh->m_verts=mm->Get_Vertex_Array();
	geomMesh->m_numPolygons=mm->Get_Polygon_Count();
	geomMesh->m_polygons=mm->Get_Polygon_Array();

	if (geomMesh->m_numVerts > MAX_SHADOW_VOLUME_VERTS)
		return FALSE;	//too many vertices to process

	//reset index of all vertices
	memset(vertParent,0xffffffff,sizeof(vertParent));
	newVertexCount=geomMesh->m_numVerts;
	//Find all duplicated vertices.
	for (j=0; j<geomMesh->m_numVerts; j++)
	{
		if (vertParent[j] != 0xffff)
			continue;	//this vertex has already been processed

		const Vector3 *v_curr=&geomMesh->m_verts[j];

		for (k=j+1; k<geomMesh->m_numVerts; k++)
		{
			Vector3 len(*v_curr - geomMesh->m_verts[k]);
			if (len.Length2() == 0)
			{	//found duplicate vertex
				vertParent[k]=j;
				newVertexCount--;	//decrease total vertices since duplicate found.
			}
		}
		vertParent[j]=j;	//first instance of new vertex
	}

	geomMesh->m_parentVerts = NEW UnsignedShort[geomMesh->m_numVerts];
	memcpy(geomMesh->m_parentVerts,vertParent,sizeof(UnsignedShort)*geomMesh->m_numVerts);
	geomMesh->m_numVerts=newVertexCount;	//adjust actual vertex count to ignore duplicates
	geomMesh->m_parentGeometry = this;

	m_meshCount=1;
	m_numTotalsVerts=newVertexCount;

	// build our neighboring polygon information
//	geomMesh->buildPolygonNeighbors();

	return TRUE;
}

Int W3DShadowGeometry::init(RenderObjClass *robj)
{
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// MEMBER DEFINITIONS /////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// W3DShadowGeometry =============================================================
// ============================================================================
W3DShadowGeometryMesh::W3DShadowGeometryMesh()
{
	// init polygon neighbor information
	m_polyNeighbors = nullptr;
	m_numPolyNeighbors = 0;
	m_parentVerts = nullptr;
	m_polygonNormals = nullptr;
}

// ~W3DShadowGeometry ============================================================
// ============================================================================
W3DShadowGeometryMesh::~W3DShadowGeometryMesh()
{
	// remove our neighbor list information allocated
	deleteNeighbors();

	delete [] m_parentVerts;
	delete [] m_polygonNormals;

}

// GetPolyNeighbor ============================================================
// Return the poly neighbor structure at the given index
// ============================================================================
PolyNeighbor *W3DShadowGeometryMesh::GetPolyNeighbor( Int polyIndex )
{
if (!m_polyNeighbors) {
	buildPolygonNeighbors();
}

	// sanity
	if( polyIndex < 0 || polyIndex >= m_numPolyNeighbors )
	{

//		DBGPRINTF(( "Invalid neighbor index '%d'\n", polyIndex ));
		assert( 0 );
		return nullptr;

	}

	return &m_polyNeighbors[ polyIndex ];

}

// buildPolygonNeighbors ======================================================
// Whenever we set a new geometry we want to build some information about
// the faces in the new geometry so that we can efficiently traverse across
// the surface to neighboring polygons
// ============================================================================
void W3DShadowGeometryMesh::buildPolygonNeighbors()
{
	Int numPolys;
	Int i, j;
	// Jani: Make sure we have polygon normals BEFORE we need them...
	buildPolygonNormals();

	// how many polygons are in our geometry
	numPolys = GetNumPolygon();

	//
	// if there are no polygons for this geometry then we should have no
	// neighbor information
	//
	if( numPolys == 0 )
	{

		//
		// if our geometry has somehow deformed and we used to have polygon
		// neighbor information we should delete it before we bail
		//
		if( m_numPolyNeighbors != 0 )
			deleteNeighbors();

		return;  // nothing to see here people, move along

	}

	//
	// in the event that this geometry can deform on the fly or we are
	// building our neighbor information for the very first time ...
	// if our current geometry has a different number of polygons than
	// we had previously calculated we need to delete and reallocate a
	// new storage space for the neighbor information
	//
	if( numPolys != m_numPolyNeighbors )
	{

		// delete the old neighbor storage
		deleteNeighbors();

		// allocate a new pool for neighbor information
		if( allocateNeighbors( numPolys ) == FALSE )
			return;

	}

	//
	// initialize all polygon neighbor information to none and assign our
	// own reference to myIndex so we know who we are
	//
	for( i = 0; i < m_numPolyNeighbors; i++ )
	{

		// assign our own identification
		m_polyNeighbors[ i ].myIndex = i;

		// assign our neighbors to none
		for( j = 0; j < MAX_POLYGON_NEIGHBORS; j++ )
			m_polyNeighbors[ i ].neighbor[ j ].neighborIndex = NO_NEIGHBOR;

	}

	// assign polygon data for each of our polygons
	for( i = 0; i < m_numPolyNeighbors; i++ )
	{
		Short poly[ 3 ];  // vertex indices for this polygon
		Short otherPoly[ 3 ];  // vertex indices for other polygon

		// get the indices of the three triangle points for this polygon
		GetPolygonIndex( i, poly );
		const Vector3& vNorm=GetPolygonNormal(i);

		// find the neighbors of this polygon
		for( j = 0; j < m_numPolyNeighbors; j++ )
		{
			Int a, b;
			Int index1, index2;
			Int index1Pos[2]; //positions of shared edge vertices in triangle list. (0,1 or 2)
			Int diff1,diff2;

			// ignore our own polygon
			if( i == j )
				continue;

			// get the vertex index information for this other polygon
			GetPolygonIndex( j, otherPoly );

			//
			// if 2 of the 3 vertex indices are the same then these polygons
			// are neighbors
			//
			//Also check if winding order of vertices on edge is opposite.
			//If vertices are in same order as our polygon, then it's
			//not a valid edge because the neighbor is flipped.

			index1 = -1;
			index2 = -1;
			for( a = 0; a < 3; a++ )
				for( b = 0; b < 3; b++ )
					if( poly[ a ] == otherPoly[ b ] )
					{

						if( index1 == -1 )
						{	index1 = poly[ a ];  // record matching index1
							index1Pos[0]=a;
							index1Pos[1]=b;
						}
						else if( index2 == -1 )
						{	//Check direction of edge in each polygon.  If they are same direction skip it.
							diff1 = a-index1Pos[0];
							diff2 = b-index1Pos[1];
							if ( ((diff1&0x80000000)^((abs(diff1)&2)<<30)) != ((diff2&0x80000000)^((abs(diff2)&2)<<30)))
							{

								const Vector3& vOtherNorm=GetPolygonNormal(j);

								//Check if the 2 polygons face in exactly opposite directions - don't allow this type of neighbor.
								if (fabs(Vector3::Dot_Product(vOtherNorm,vNorm) + 1.0f) <= 0.01f)
									continue;

								index2 = poly[ a ];  // record matching index2
							}
							else
								continue;
						}
						else
						{//This is the same poly facing opposite direction.	//assert( 0 );  // should never match 3 vertices!
							index1=index2=-1;
							continue;
						}
					}
			if( index1 != -1 && index2 != -1  )
			{
				//
				// the polygon j is a neighbor of our polygon i, put the j
				// index into the first free neighbor slot for polygon i
				//
				for( a = 0; a < MAX_POLYGON_NEIGHBORS; a++ )
					if( m_polyNeighbors[ i ].neighbor[ a ].neighborIndex == NO_NEIGHBOR )
					{

						// record the neighbor index
						m_polyNeighbors[ i ].neighbor[ a ].neighborIndex = j;

						// record the sharded edge vertex indices
						m_polyNeighbors[ i ].neighbor[ a ].neighborEdgeIndex[ 0 ] = index1;
						m_polyNeighbors[ i ].neighbor[ a ].neighborEdgeIndex[ 1 ] = index2;

						break;  // exit for a

					}

				//
				// error condition, if our counter a is at the max number
				// of neighbors, which is 3 for a triangle mesh, we did something
				// wrong here because that would mean we found a 4th match!
				//
				if (a == MAX_POLYGON_NEIGHBORS)
				{
//					Vector3 pv[3];
//					char errorText[255];

//					GetVertex (poly[0], &pv[0]);
//					GetVertex (poly[1], &pv[1]);
//					GetVertex (poly[2], &pv[2]);

//					pv[0] = pv[0] + pv[1] + pv[2];
//					pv[0] /= 3.0f;	//find center of polygon

//					sprintf(errorText,"%s: Shadow Polygon with too many neighbors at %f,%f,%f",m_parentGeometry->Get_Name(),pv[0].X,pv[0].Y,pv[0].Z);
//					DEBUG_LOG(("****%s Shadow Polygon with too many neighbors at %f,%f,%f",m_parentGeometry->Get_Name(),pv[0].X,pv[0].Y,pv[0].Z));
//					DEBUG_ASSERTCRASH(a != MAX_POLYGON_NEIGHBORS,(errorText));
				}

			}

		}

	}

}

// allocateNeighbors ==========================================================
// Allocate storage for the polygon neighbors and record its size
// ============================================================================
Bool W3DShadowGeometryMesh::allocateNeighbors( Int numPolys )
{

	// assure we're not re-allocating without deleting
	assert( m_numPolyNeighbors == 0 );
	assert( m_polyNeighbors == nullptr );

	// allocate the list
	m_polyNeighbors = NEW PolyNeighbor[ numPolys ];
	if( m_polyNeighbors == nullptr )
	{

//		DBGPRINTF(( "Unable to allocate polygon neighbors\n" ));
		assert( 0 );
		return FALSE;

	}

	// list is now actually allocated
	m_numPolyNeighbors = numPolys;

	return TRUE;  // success!

}

// deleteNeighbors ============================================================
// Delete all polygon neighbor storage and information
// ============================================================================
void W3DShadowGeometryMesh::deleteNeighbors()
{

	// delete list
	delete [] m_polyNeighbors;
	m_polyNeighbors = nullptr;
	m_numPolyNeighbors = 0;

	// sanity error checking
	assert( m_numPolyNeighbors == 0 );
	assert( m_polyNeighbors == nullptr );

}

//#include "Common/ThingTemplate.h"

// updateOptimalExtrusionPadding ==============================================
// Use raycasting to figure out a shadow extrusion length that guarantees that
// the highest point of the object is extruded long enough to hit some ground.
// This is a very slow operation so only do once for static non-moving objects.
// ============================================================================
void W3DVolumetricShadow::updateOptimalExtrusionPadding()
{
	if (m_robj)
	{
//		DrawableInfo *drawInfo=(DrawableInfo *)m_robj->Get_User_Data();
//		Drawable *draw = drawInfo->m_drawable;

//		if (strstr(draw->getTemplate()->getName().str(),"Right02") != 0)
//			draw = draw;	//debug code for China06 wacky bridge shadow

		// get the light
		Vector3 lightPosWorld=TheW3DShadowManager->getLightPosWorld(0);

		// check if object has a limit/clamp on shadow length and adjust light
		// position of necessary.
		if (m_shadowLengthScale)
		{	//Find light's distance from origin in xy plane
			Real lightXYDistance = sqrt(lightPosWorld.X*lightPosWorld.X + lightPosWorld.Y * lightPosWorld.Y);
			Real newZ=lightXYDistance*m_shadowLengthScale;

			if (newZ > lightPosWorld.Z)
			{	//clamped z component is higher than actual light position allows so adjust it.
				lightPosWorld.Z = newZ;
			}
		}

		// find maximum shadow length which will not cause any corners of the object's bounding box
		// to cast shadows that drop significantly below the object's base.  This will help avoid
		// artifacts when we have an object on a cliff/hill casting shadows onto the ground below.
		// We need this hack because the terrain does not cast shadows and looks weird when objects
		//	sitting on an incline cast shadows down below.
		Vector3 objPos=m_robj->Get_Position();
		Vector3 lastValidTerrainPoint = objPos;
		Real baseGroundHeight=objPos.Z;
		const AABoxClass &box=m_robj->Get_Bounding_Box();
		Vector3 lightRay,shadowRay;
		LineSegClass lineseg;
		CastResultStruct result;
		Vector3 Corners[4];

		RayCollisionTestClass raytest(lineseg,&result);

		//Get vertices of top of bounding box since they will generate the longest shadow
		Corners[0]=box.Center+box.Extent;	//top right corner
		Corners[1]=Corners[0];
		Corners[1].X -= 2.0f*box.Extent.X;		//top left corner
		Corners[2]=Corners[1];
		Corners[2].Y -= 2.0f*box.Extent.Y;		//bottom left corner
		Corners[3]=Corners[2];
		Corners[3].X += 2.0f*box.Extent.X;		//bottom right corner

		//find the corner that causes the longest shadow projection
		//and clamp light position to make sure it falls on even ground about
		//the same height as the object's base.
		for (Int i=0; i<4; i++)
		{
			//Cast ray from top volume corners onto ground plane
			lightRay = Corners[i] - lightPosWorld;	//vector light to corner
			lightRay.Normalize();

			raytest.Ray.Set(Corners[i],Corners[i]+lightRay*MAX_EXTRUSION_LENGTH);
			result.Reset();

			//find out where this ray intersects terrain.
			if (TheTerrainRenderObject->Cast_Ray(raytest) && !raytest.Result->StartBad)
			{	//Found intersection point where shadow has its maximum length.  Do a quick
				//search to see if terrain falls significantly below the height of the object
				//anywhere between the base and the intersection point.  If so, we either need
				//to extend shadow extrusion or make the light angle more vertical.

				shadowRay.Set(result.ContactPoint-Corners[i]);	//vector from object corner to terrain intersection.
				shadowRay.Z = 0;	//remove z-component since we'll be sampling along the xy plane.

				//walk along the shadow/light direction vector looking for large dips - indicating object
				//is on hill or cliff.
				Real len=shadowRay.Length();
				Int  numSteps=REAL_TO_INT_CEIL(len/SHADOW_SAMPLING_INTERVAL);
				Real stepSize = 1.0f/(Real)numSteps;
				Vector3 terrainPoint;

				Real t=stepSize;
				for (Int j=0; j<numSteps; j++)
				{
					terrainPoint = Corners[i] + shadowRay*t;
					terrainPoint.Z=0;	//ignore height

					Real terrainHeight=TheTerrainRenderObject->getHeightMapHeight(terrainPoint.X,terrainPoint.Y,nullptr);
					if (terrainHeight < (objPos.Z - MAX_SHADOW_EXTRUSION_UNDER_OBJECT_BEFORE_CLAMP))	//check if terrain dips more than 10 units under object.
					{
						if (j == 0)	//this is the initial point so object must be right on the edge of a cliff.
						{	baseGroundHeight = terrainHeight;	//force extrusion all the way down cliff.
							Real tanAngle=tan(OVERHANGING_OBJECT_CLAMP_ANGLE);	//clamp to about 89 degrees or close to vertical lightpos.
							setShadowLengthScale(tanAngle);	//update the clamp angle to shorted shadow enough so this corner on flat ground.
							break;
						}

						//Find ray from last valid terrain contact point to object box corner.
						Vector3 clampRay(Corners[i]-lastValidTerrainPoint);
						Real clampAngle=asin(clampRay.Z/clampRay.Length());
						if (clampAngle >= (PI/2.0f) || clampAngle <= 0)
							clampAngle = OVERHANGING_OBJECT_CLAMP_ANGLE;	//clamp to about 89 degrees or close to vertical lightpos.
						Real tanAngle=tan(clampAngle);
						if (tanAngle > m_shadowLengthScale)
							setShadowLengthScale(tanAngle);	//update the clamp angle to shorted shadow enough so this corner on flat ground.
						break;
					}

					if (terrainHeight < baseGroundHeight)
					{	baseGroundHeight = terrainHeight;	//point was below object but within safety margin so record it's position.
						lastValidTerrainPoint = terrainPoint;
						lastValidTerrainPoint.Z = baseGroundHeight;
					}

					t+=stepSize;
				}
			}
		}


		m_extraExtrusionPadding = objPos.Z - baseGroundHeight + SHADOW_EXTRUSION_BUFFER;

		DEBUG_ASSERTCRASH(m_extraExtrusionPadding <= (255.0f*MAP_HEIGHT_SCALE),("Warning: Volumetric Shadow UpdateOptimalExtrusionPadding too large"));
	}
}

// getRenderCost ============================================================
// Returns number of draw calls for this shadow.
// ============================================================================
#if defined(RTS_DEBUG)
void W3DVolumetricShadow::getRenderCost(RenderCost & rc) const
{
	Int drawCount = 0;

	if (m_geometry && m_isEnabled && !m_isInvisibleEnabled && TheGlobalData->m_useShadowVolumes)
	{
		Int i,j;

		HLodClass *hlod=(HLodClass *)m_robj;
		MeshClass *mesh;
		Int meshIndex;

		for( i = 0; i < MAX_SHADOW_LIGHTS; i++ )
		{
			for (j=0; j<m_geometry->getMeshCount(); j++)
			{
				meshIndex=m_geometry->getMesh(j)->m_meshRobjIndex;

				if (meshIndex >= 0)
					mesh = (MeshClass *)hlod->Peek_Lod_Model(0,meshIndex);
				else
					mesh = (MeshClass *)m_robj;

				if (mesh && mesh->Is_Not_Hidden_At_All())
						drawCount++;
			}
		}
	}

	rc.addShadowDrawCalls(drawCount*2);
}
#endif

/************************************ New Buffered Rendering Code ************************/
void W3DVolumetricShadow::RenderVolume(Int meshIndex, Int lightIndex)
{
	HLodClass *hlod=(HLodClass *)m_robj;
	MeshClass *mesh=nullptr;

	Int meshRobjIndex=m_geometry->getMesh(meshIndex)->m_meshRobjIndex;

	if (meshRobjIndex >= 0)
		mesh = (MeshClass *)hlod->Peek_Lod_Model(0,meshRobjIndex);
	else
		mesh = (MeshClass *)m_robj;

	if (mesh)
	{
#ifdef SV_DEBUG_BOUNDS
			RenderMeshVolumeBounds(meshIndex,lightIndex, &mesh->Get_Transform());
#endif
			if (m_shadowVolume[0][ meshIndex ]->GetFlags() & SHADOW_DYNAMIC)
				RenderDynamicMeshVolume(meshIndex,lightIndex,&mesh->Get_Transform());
			else
				RenderMeshVolume(meshIndex,lightIndex,&mesh->Get_Transform());
	}
}

void W3DVolumetricShadow::RenderMeshVolume(Int meshIndex, Int lightIndex, const Matrix3D *meshXform)
{
	Geometry *geometry;
	Int numVerts, numPolys, numIndex;

	//Get D3D Device used by W3D for quicker access.
	LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();

	if (!m_pDev)
		return;

	geometry = m_shadowVolume[lightIndex][ meshIndex ];

	//
	// if our count is out of sync with our geometry data something
	// is wrong here
	//
	assert( geometry );

	// get geometry requirements
	numVerts = geometry->GetNumActiveVertex();
	numPolys = geometry->GetNumActivePolygon();
	numIndex = numPolys * 3;

	// reject shadows with no data
	if( numVerts == 0 || numPolys == 0 )
		return;

	// GeneralsX @bugfix BenderAI 21/03/2026 Align mesh world transform with Generals base shadow path.
	D3DMATRIX dxmWorld = To_D3DMATRIX(*meshXform);
	m_pDev->SetTransform(D3DTS_WORLD, &dxmWorld);

	W3DBufferManager::W3DVertexBufferSlot *vbSlot=m_shadowVolumeVB[lightIndex][ meshIndex ];
	if (!vbSlot)
		return;
	if (vbSlot->m_VB->m_DX8VertexBuffer->Get_DX8_Vertex_Buffer() != lastActiveVertexBuffer)
	{	lastActiveVertexBuffer=vbSlot->m_VB->m_DX8VertexBuffer->Get_DX8_Vertex_Buffer();
		m_pDev->SetStreamSource(0,lastActiveVertexBuffer,
			vbSlot->m_VB->m_DX8VertexBuffer->FVF_Info().Get_FVF_Size());	//12 bytes per vertex.
	}

	DEBUG_ASSERTCRASH(vbSlot->m_size >= numVerts,("Overflowing Shadow Vertex Buffer Slot"));

	W3DBufferManager::W3DIndexBufferSlot *ibSlot=m_shadowVolumeIB[lightIndex][ meshIndex ];
	if (!ibSlot)
		return;

	DEBUG_ASSERTCRASH(ibSlot->m_size >= numIndex,("Overflowing Shadow Index Buffer Slot"));

	m_pDev->SetIndices(ibSlot->m_IB->m_DX8IndexBuffer->Get_DX8_Index_Buffer(),vbSlot->m_start);

	if (DX8Wrapper::_Is_Triangle_Draw_Enabled())
	{
		Debug_Statistics::Record_DX8_Polys_And_Vertices(numPolys,numVerts,ShaderClass::_PresetOpaqueShader);
		m_pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,numVerts,ibSlot->m_start,numPolys);
	}

}

void W3DVolumetricShadow::RenderDynamicMeshVolume(Int meshIndex, Int lightIndex, const Matrix3D *meshXform)
{
	Geometry *geometry;
	Int numVerts, numPolys, numIndex;
	SHADOW_DYNAMIC_VOLUME_VERTEX* pvVertices;
	UnsignedShort *pvIndices;

	//Get D3D Device used by W3D for quicker access.
	LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();

	if (!m_pDev)
		return;


	geometry = m_shadowVolume[lightIndex][ meshIndex ];

	//
	// if our count is out of sync with our geometry data something
	// is wrong here
	//
	assert( geometry );

	// get geometry requirements
	numVerts = geometry->GetNumActiveVertex();
	numPolys = geometry->GetNumActivePolygon();
	numIndex = numPolys * 3;

	// reject shadows with no data
	if( numVerts == 0 || numPolys == 0 )
		return;


	if (nShadowVertsInBuf > (SHADOW_VERTEX_SIZE-numVerts))	//check if room for model verts
	{	//flush the buffer by drawing the contents and re-locking again
		if (shadowVertexBufferD3D->Lock(0,numVerts*sizeof(SHADOW_DYNAMIC_VOLUME_VERTEX),(unsigned char**)&pvVertices,D3DLOCK_DISCARD) != D3D_OK)
			return;
		nShadowVertsInBuf=0;
		nShadowStartBatchVertex=0;
	}
	else
	{	if (shadowVertexBufferD3D->Lock(nShadowVertsInBuf*sizeof(SHADOW_DYNAMIC_VOLUME_VERTEX),numVerts*sizeof(SHADOW_DYNAMIC_VOLUME_VERTEX), (unsigned char**)&pvVertices,D3DLOCK_NOOVERWRITE) != D3D_OK)
			return;
	}
#ifdef SV_DEBUG
	srand(0x1345465);
#endif
	if(pvVertices)
	{
#ifdef SV_DEBUG
		for (Int i=0; i<numVerts; i++)
		{
			(*((Vector3 *)pvVertices))=*geometry->GetVertex(i);	//cast is valid since both start with xyz
			pvVertices->diffuse=(rand()%255) | ((rand()%255)<<8) | ((rand()%255)<<16);
			pvVertices++;
		}
#else
		memcpy(pvVertices,geometry->GetVertex(0),numVerts*sizeof(SHADOW_DYNAMIC_VOLUME_VERTEX));
#endif
	}

	shadowVertexBufferD3D->Unlock();

	if (nShadowIndicesInBuf > (SHADOW_INDEX_SIZE-numIndex))	//check if room for model verts
	{	//flush the buffer by drawing the contents and re-locking again
		if (shadowIndexBufferD3D->Lock(0,numIndex*sizeof(short),(unsigned char**)&pvIndices,D3DLOCK_DISCARD) != D3D_OK)
			return;
		nShadowIndicesInBuf=0;
		nShadowStartBatchIndex=0;
	}
	else
	{	if (shadowIndexBufferD3D->Lock(nShadowIndicesInBuf*sizeof(short),numIndex*sizeof(short), (unsigned char**)&pvIndices,D3DLOCK_NOOVERWRITE) != D3D_OK)
			return;
	}


	if(pvIndices)
	{
		memcpy(pvIndices,geometry->GetPolygonIndex(0,(short *)pvIndices),numPolys*3*sizeof(short));
	}

	shadowIndexBufferD3D->Unlock();

	m_pDev->SetIndices(shadowIndexBufferD3D,nShadowStartBatchVertex);

	// GeneralsX @bugfix BenderAI 21/03/2026 Use stable DX8 matrix conversion for animated mesh shadow rendering.
	D3DMATRIX dxmWorld = To_D3DMATRIX(*meshXform);
	m_pDev->SetTransform(D3DTS_WORLD, &dxmWorld);

	// GeneralsX @bugfix BenderAI 11/04/2026 Rebind dynamic shadow vertex stream to avoid stale VB state on animated casters.
	if (shadowVertexBufferD3D != lastActiveVertexBuffer)
	{
		m_pDev->SetStreamSource(0,shadowVertexBufferD3D,sizeof(SHADOW_DYNAMIC_VOLUME_VERTEX));
		lastActiveVertexBuffer = shadowVertexBufferD3D;
	}

	if (DX8Wrapper::_Is_Triangle_Draw_Enabled())
	{
		Debug_Statistics::Record_DX8_Polys_And_Vertices(numPolys,numVerts,ShaderClass::_PresetOpaqueShader);
		m_pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,numVerts,nShadowStartBatchIndex,numPolys);
	}

	nShadowVertsInBuf += numVerts;
	nShadowStartBatchVertex=nShadowVertsInBuf;

	nShadowIndicesInBuf += numIndex;
	nShadowStartBatchIndex=nShadowIndicesInBuf;
}

/** Debug function to draw bounding boxes around shadow volumes */
void W3DVolumetricShadow::RenderMeshVolumeBounds(Int meshIndex, Int lightIndex, const Matrix3D *meshXform)
{
	Geometry *geometry;
	Int numVerts, numPolys, numIndex;
	SHADOW_DYNAMIC_VOLUME_VERTEX* pvVertices;
	UnsignedShort *pvIndices;
	// Vertex Positions as a function of the box extents
	static Vector3						_BoxVerts[8] =
	{
		Vector3(  1.0f, 1.0f, 1.0f ),		// +z ring of 4 verts
		Vector3( -1.0f, 1.0f, 1.0f ),
		Vector3( -1.0f,-1.0f, 1.0f ),
		Vector3(  1.0f,-1.0f, 1.0f ),

		Vector3(  1.0f, 1.0f,-1.0f ),		// -z ring of 4 verts;
		Vector3( -1.0f, 1.0f,-1.0f ),
		Vector3( -1.0f,-1.0f,-1.0f ),
		Vector3(  1.0f,-1.0f,-1.0f ),
	};
	// Face Connectivity
	static Vector3i					_BoxFaces[12] =
	{
		Vector3i( 0,1,2 ),		// +z faces
		Vector3i( 0,2,3 ),
		Vector3i( 4,7,6 ),		// -z faces
		Vector3i( 4,6,5 ),
		Vector3i( 0,3,7 ),		// +x faces
		Vector3i( 0,7,4 ),
		Vector3i( 1,5,6 ),		// -x faces
		Vector3i( 1,6,2 ),
		Vector3i( 4,5,1 ),		// +y faces
		Vector3i( 4,1,0 ),
		Vector3i( 3,2,6 ),		// -y faces
		Vector3i( 3,6,7 )
	};

	static Vector3 verts[8];

	//Get D3D Device used by W3D for quicker access.
	LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();

	if (!m_pDev)
		return;

	Vector3 meshPosition;
	meshXform->Get_Translation(&meshPosition);	//current mesh position

	geometry = m_shadowVolume[lightIndex][ meshIndex ];
	AABoxClass &aab=geometry->getBoundingBox();

	// compute the vertex positions
	meshPosition += aab.Center;	//get world space position of bounding box

	for (int ivert=0; ivert<8; ivert++)
	{
		verts[ivert].X = meshPosition.X + _BoxVerts[ivert][0] * aab.Extent.X;
		verts[ivert].Y = meshPosition.Y + _BoxVerts[ivert][1] * aab.Extent.Y;
		verts[ivert].Z = meshPosition.Z + _BoxVerts[ivert][2] * aab.Extent.Z;
	}

	//
	// if our count is out of sync with our geometry data something
	// is wrong here
	//
	assert( geometry );

	// get geometry requirements
	numVerts = 8;
	numPolys = 12;
	numIndex = numPolys * 3;

	// reject shadows with no data
	if( numVerts == 0 || numPolys == 0 )
		return;


	if (nShadowVertsInBuf > (SHADOW_VERTEX_SIZE-numVerts))	//check if room for model verts
	{	//flush the buffer by drawing the contents and re-locking again
		if (shadowVertexBufferD3D->Lock(0,numVerts*sizeof(SHADOW_DYNAMIC_VOLUME_VERTEX),(unsigned char**)&pvVertices,D3DLOCK_DISCARD) != D3D_OK)
			return;
		nShadowVertsInBuf=0;
		nShadowStartBatchVertex=0;
	}
	else
	{	if (shadowVertexBufferD3D->Lock(nShadowVertsInBuf*sizeof(SHADOW_DYNAMIC_VOLUME_VERTEX),numVerts*sizeof(SHADOW_DYNAMIC_VOLUME_VERTEX), (unsigned char**)&pvVertices,D3DLOCK_NOOVERWRITE) != D3D_OK)
			return;
	}
	srand(0x1345465);
	if(pvVertices)
	{	for (Int i=0; i<8; i++)
		{
			pvVertices->x=verts[i][0];
			pvVertices->y=verts[i][1];
			pvVertices->z=verts[i][2];
#ifdef SV_DEBUG
			pvVertices->diffuse=(rand()%255) | ((rand()%255)<<8) | ((rand()%255)<<16);
#endif
			pvVertices++;
		}
	}

	shadowVertexBufferD3D->Unlock();

	if (nShadowIndicesInBuf > (SHADOW_INDEX_SIZE-numIndex))	//check if room for model verts
	{	//flush the buffer by drawing the contents and re-locking again
		if (shadowIndexBufferD3D->Lock(0,numIndex*sizeof(short),(unsigned char**)&pvIndices,D3DLOCK_DISCARD) != D3D_OK)
			return;
		nShadowIndicesInBuf=0;
		nShadowStartBatchIndex=0;
	}
	else
	{	if (shadowIndexBufferD3D->Lock(nShadowIndicesInBuf*sizeof(short),numIndex*sizeof(short), (unsigned char**)&pvIndices,D3DLOCK_NOOVERWRITE) != D3D_OK)
			return;
	}


	if(pvIndices)
	{
		for (Int i=0; i<numPolys; i++,pvIndices+=3)
		{
			pvIndices[0] = _BoxFaces[i][0];
			pvIndices[1] = _BoxFaces[i][1];
			pvIndices[2] = _BoxFaces[i][2];
		}
	}

	shadowIndexBufferD3D->Unlock();

	m_pDev->SetIndices(shadowIndexBufferD3D,nShadowStartBatchVertex);


	//todo: replace this with mesh transform
	// GeneralsX @bugfix BenderAI 21/03/2026 Keep bounds pass consistent with Generals matrix convention.
	Matrix4x4 mWorld(1);	//identity since boxes are pre-transformed to world space.
	D3DMATRIX dxmWorld = To_D3DMATRIX(mWorld);
	m_pDev->SetTransform(D3DTS_WORLD, &dxmWorld);

	m_pDev->SetStreamSource(0,shadowVertexBufferD3D,sizeof(SHADOW_DYNAMIC_VOLUME_VERTEX));
	m_pDev->SetVertexShader(SHADOW_DYNAMIC_VOLUME_FVF);

	m_pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,numVerts,nShadowStartBatchIndex,numPolys);

	nShadowVertsInBuf += numVerts;
	nShadowStartBatchVertex=nShadowVertsInBuf;

	nShadowIndicesInBuf += numIndex;
	nShadowStartBatchIndex=nShadowIndicesInBuf;
}

// Shadow =====================================================================
// Shadow default constructor
// ============================================================================
W3DVolumetricShadow::W3DVolumetricShadow()
{
	Int i,j;

	m_next = nullptr;
	m_geometry = nullptr;
	m_shadowLengthScale = 0.0f;
	m_extraExtrusionPadding = 0.0f;
	m_robj = nullptr;
	m_isEnabled = TRUE;
	m_isInvisibleEnabled = FALSE;

	for (j=0; j < MAX_SHADOW_CASTER_MESHES; j++)
	{	m_numSilhouetteIndices[j] = 0;
		m_maxSilhouetteEntries[j] = 0;
		m_silhouetteIndex[j] = nullptr;
		m_shadowVolumeCount[j] = 0;
	}

	for( i = 0; i < MAX_SHADOW_LIGHTS; i++ )
	{
		for (j=0; j < MAX_SHADOW_CASTER_MESHES; j++)
		{
			m_shadowVolume[ i ][j] = nullptr;
			m_shadowVolumeVB[i][j] = nullptr;
			m_shadowVolumeIB[i][j] = nullptr;
			m_shadowVolumeRenderTask[i][j].m_parentShadow = this;
			m_shadowVolumeRenderTask[i][j].m_meshIndex = (UnsignedByte)j;
			m_shadowVolumeRenderTask[i][j].m_lightIndex = (UnsignedByte)i;
			m_objectXformHistory[ i ][j].Make_Identity();
			m_lightPosHistory[ i ][j] = Vector3(0,0,0);
		}
	}

}

// ~W3DVolumetricShadow ====================================================================
// W3DVolumetricShadow destructor
// ============================================================================
W3DVolumetricShadow::~W3DVolumetricShadow()
{
	Int i,j;

	// we must free any silhouette data allocated
	for (j = 0; j < MAX_SHADOW_CASTER_MESHES; j++)
		deleteSilhouette(j);

	// free any shadow volume data
	for( i = 0; i < MAX_SHADOW_LIGHTS; i++ )
	{	for (j = 0; j < MAX_SHADOW_CASTER_MESHES; j++)
		{
			delete m_shadowVolume[i][j];
			if( m_shadowVolumeVB[i][j])
				TheW3DBufferManager->releaseSlot(m_shadowVolumeVB[i][j]);
			if( m_shadowVolumeIB[i][j])
				TheW3DBufferManager->releaseSlot(m_shadowVolumeIB[i][j]);
		}
	}

	if (m_geometry)
		REF_PTR_RELEASE(m_geometry);

	m_geometry=nullptr;
	m_robj=nullptr;

}

void W3DVolumetricShadow::SetGeometry( W3DShadowGeometry *geometry )
{

	Short numPrevVertices = 0;
	Short numNewVertices = 0;

	//
	// our geometry has changed, we need to allocate enough memory for the
	// silhouette data.  If silhouette data is present it must be reallocated
	// to accomoddate the new size if smaller
	//

	// if we had previous geometry how many vertices did it have

	for (Int i=0; i<MAX_SHADOW_CASTER_MESHES; i++)
	{
		if( m_geometry )
			numPrevVertices = m_geometry->getMesh(i)->GetNumVertex();

		// now many vertices does our new geometry have
		if( geometry )
			numNewVertices = geometry->getMesh(i)->GetNumVertex();

		//
		// TODO: Colin, may want to change this in the future
		// if our new geometry requires more memory allocate it, if it requires
		// less we'll leave it around for future switches in geometry
		//
		if( numNewVertices > numPrevVertices )
		{

			deleteSilhouette(i);
			if( allocateSilhouette(i, numNewVertices ) == FALSE )
				return;

		}
	}

	// assign the new geometry, possible over an old geometry
	m_geometry = geometry;

}

/**Called once per frame for each object, when necessary it will reconstruct
 the shadow volume for this shadow from the silhouette of the geometry
 and any light sources
*/
void W3DVolumetricShadow::Update()
{
	static Int currentTime, lastTime, delay = 0;
	// OBJECT_PILE
	// static Vector3 originCompareVector(0,0,9999);
	static Vector3 originCompareVector(0,0,0);
	Vector3 pos;

	// sanity
	if( m_geometry == nullptr)
		return;

	//
	// for now we will just rebuild a shadow volume every so often, this
	// should be changed to be built only when the light angle sufficiently
	// changes or when the object rotation sufficiently changes
	//

//	currentTime = timeGetTime();
//	if( currentTime - lastTime >= delay )
	{
		pos=m_robj->Get_Position();
		if (pos == originCompareVector)
		{	//the transform on this object was never set so we can't make any determination
			//if it's visible or what the shadow looks like.
			return;
		}
		//Check if this is a "flying" unit.  Flying units are defined as anything that moves more than
		//AIRBORNE_UNIT_GROUND_DELTA world units above ground.  This will allow "jumping" units like rocket
		//buggies from being flagged as "flying".  We minimize the number of flying units because their shadow
		//volumes are extruded longer to reach the lowest point on the map.  Regular shadows only extend far
		//enough to reach the base of the model (presumes base is already touching the ground).
		Real groundHeight;
		if (TheTerrainLogic)
			groundHeight=TheTerrainLogic->getGroundHeight(pos.X,pos.Y);	//logic knows about bridges so use if available.
		else
			groundHeight=TheTerrainRenderObject->getHeightMapHeight(pos.X,pos.Y, nullptr);
   		if (fabs(pos.Z - groundHeight) >= AIRBORNE_UNIT_GROUND_DELTA)
   		{
 			Real extent = MAX_SHADOW_LENGTH_EXTRA_AIRBORNE_SCALE_FACTOR * m_robjExtent;
 			if (WWMath::Fabs(pos.X - bcX) > (beX + extent) ||
 				WWMath::Fabs(pos.Y - bcY) > (beY + extent) ||
 				WWMath::Fabs(pos.Z - bcZ) > (beZ + extent))
 				return;	//shadow can't be visible so no point in updating.

			//this unit is above ground, extend shadow volume to reach lowest point on the terrain plus extra bit to make
			//sure shadow goes under ground.
   			updateVolumes(fabs(pos.Z - TheTerrainRenderObject->getMinHeight()) + SHADOW_EXTRUSION_BUFFER);
   		}
   		else
 		{	//normal object that is not floating above ground so we don't need to extend the shadow lower than the object's
			//base since it should be sitting directly at ground level.

 			if (WWMath::Fabs(pos.X - bcX) > (beX + m_robjExtent) ||
 				WWMath::Fabs(pos.Y - bcY) > (beY + m_robjExtent) ||
 				WWMath::Fabs(pos.Z - bcZ) > (beZ + m_robjExtent))
 				return;	//shadow can't be visible so no point in updating.

				//check if this object has never had it's extrusion length updated.  Will only be true for
				//immobile objects because finding an optimal extrusion length is expensive.
				if (!m_extraExtrusionPadding)
					updateOptimalExtrusionPadding();

   			updateVolumes(m_extraExtrusionPadding);
 		}


	//	floorZ = 2.0f;	//lower slightly so shadows go under ground.


		// update delay time
		lastTime = currentTime;
	}

}

/** Update shadow volumes belonging to all meshes of this shadow caster.
*	Use zoffset to extend shadows below object's base by given amount.
*/
void W3DVolumetricShadow::updateVolumes(Real zoffset)
{
	Int i,j;

	HLodClass *hlod=(HLodClass *)m_robj;
	MeshClass *mesh;
	static AABoxClass aaBox;
	static SphereClass sphere;
	Int meshIndex;

	DEBUG_ASSERTCRASH(hlod != nullptr,("updateVolumes : hlod is null!"));

	Bool parentVis=m_robj->Is_Really_Visible();

	for( i = 0; i < MAX_SHADOW_LIGHTS; i++ )
	{
		for (j=0; j<m_geometry->getMeshCount(); j++)
		{
			meshIndex=m_geometry->getMesh(j)->m_meshRobjIndex;

			if (meshIndex >= 0)
				mesh = (MeshClass *)hlod->Peek_Lod_Model(0,meshIndex);
			else
				mesh = (MeshClass *)m_robj;

			if (mesh)
			{
				if (!mesh->Is_Not_Hidden_At_All())
					continue;

				/**@todo: Getting the transform of the mesh may be forcing a full hierarchy evaluation.
					Expensive for off-screen models... do we really need this?	*/
				//Extend floor of model by 'zoffset' to compensate for flying units.
				updateMeshVolume(j, i, &mesh->Get_Transform(), mesh->Get_Bounding_Box(),m_robj->Get_Position().Z - zoffset);
				//update visibility if not set yet
				if (m_shadowVolume[i][j])
				{
					if(m_shadowVolume[i][j]->getVisibleState() ==	Geometry::STATE_UNKNOWN)
					{	//Updating the mesh volume didn't update the visibility, must do it here.
						//First check against bounding sphere
						if (parentVis)
 						{ //parent is visible so most likely all sub_objects are also visible so probably all shadows also visible.
 						  //skip additional visibility tests
 							m_shadowVolume[i][j]->setVisibleState(Geometry::STATE_VISIBLE);
 						}
 						else
						{	sphere=m_shadowVolume[i][j]->getBoundingSphere();
							sphere.Center += mesh->Get_Transform().Get_Translation();
							CollisionMath::OverlapType result=CollisionMath::Overlap_Test(*shadowCameraFrustum,sphere);
							if (result == CollisionMath::OVERLAPPED)
							{	//do a more accurate test against bounding box.
								aaBox=m_shadowVolume[i][j]->getBoundingBox();
								aaBox.Translate(mesh->Get_Transform().Get_Translation());	//translate bounding box to world space.
								if (CollisionMath::Overlap_Test(*shadowCameraFrustum,aaBox) != CollisionMath::OUTSIDE)
									m_shadowVolume[i][j]->setVisibleState(Geometry::STATE_VISIBLE);
								else
									m_shadowVolume[i][j]->setVisibleState(Geometry::STATE_INVISIBLE);
							}
							else
								m_shadowVolume[i][j]->setVisibleState((Geometry::VisibleState)result);
						}
					}
					if (m_shadowVolume[i][j]->getVisibleState() ==	Geometry::STATE_VISIBLE)
					{	//shadow volume is visible.  Add it to list of rendertasks.
						W3DBufferManager::W3DVertexBufferSlot *vbSlot=m_shadowVolumeVB[i][j];
						if (vbSlot)
						{	//add to static mesh volume list.
							W3DBufferManager::W3DRenderTask *oldTask=vbSlot->m_VB->m_renderTaskList;
							vbSlot->m_VB->m_renderTaskList=&m_shadowVolumeRenderTask[i][j];
							vbSlot->m_VB->m_renderTaskList->m_nextTask=oldTask;
						}
						else
						{
							TheW3DVolumetricShadowManager->addDynamicShadowTask(&m_shadowVolumeRenderTask[i][j]);
						}
					}
				}
			}
		}
	}
}

/*floorZ is the assumed ground height below the model.  The code will try to extrude shadows just long enough to hit this point in order
to reduce fill rate usage.*/
void W3DVolumetricShadow::updateMeshVolume(Int meshIndex, Int lightIndex, const Matrix3D *meshXform, const AABoxClass &meshBox, float floorZ )
{
	Vector3 lightPosObject;
	Matrix4x4 worldToObject;
	Vector3 objectCenter;
	Vector3 toLight;
	Vector3 toPrevLight;
	Vector3 prevPosToLight;
	Vector3 lightPosWorld;
	Vector3 prevObjPosition;
	//Figuring out if mesh has rotated is cheaper (no normalization) than figuring out if light angle has changed.
	//So we divide the 2 tests.  Also, our light (sun) almost never moves so no second test needed at all.
	Bool isMeshRotating = false;	//flag if mesh has rotated since last update. Translation doesn't matter for infinite light source.
	Bool isLightMoving = false;	//flag if light has moved since last update.

	Matrix4x4 objectToWorld(*meshXform);
	Matrix4x4 *prevXForm=&m_objectXformHistory[ lightIndex ][meshIndex];

	//
	// build the shadow silhouette and construct shadow volume from
	// this light location.  The for loop wrapped around this is
	// theoretical code for future enhancements of multiple lights that
	// cast shadows
	//

#ifdef CNC3 //(gth) numerical error requires that the axis vectors be normalized...

	//When dealing with infinite light sources, we can assume that the shadow doesn't
	//change much based on object position.  Only the orientation to light matters.
	Vector3 va = (Vector3 &)(*prevXForm)[0];
	Vector3 vb = (Vector3 &)objectToWorld[0];
	va.Normalize();
	vb.Normalize();
	Real cosAngle = WWMath::Fabs(Vector3::Dot_Product(va,vb));

	if (cosAngle >= cosAngleToCare)
	{

		va = (Vector3 &)(*prevXForm)[1];
		vb = (Vector3 &)objectToWorld[1];
		va.Normalize();
		vb.Normalize();
		cosAngle = WWMath::Fabs(Vector3::Dot_Product(va,vb));

		if (cosAngle >= cosAngleToCare)
		{
			va = (Vector3 &)(*prevXForm)[2];
			vb = (Vector3 &)objectToWorld[2];
			va.Normalize();
			vb.Normalize();
			cosAngle = WWMath::Fabs(Vector3::Dot_Product(va,vb));
			if (cosAngle < cosAngleToCare)
				isMeshRotating=true;
		}
		else
			isMeshRotating =true;
	}
	else
		isMeshRotating =true;


#else // CNC3 (old generals code)

#ifdef ASSUME_NEAR_LIGHTSOURCE
	if (memcmp(&objectToWorld,prevXForm,sizeof(objectToWorld)))
		isMeshRotating = true; //mesh transform has not changed since last update.
#else
	//When dealing with infinite light sources, we can assume that the shadow doesn't
	//change much based on object position.  Only the orientation to light matters.
	Real cosAngle = fabs (Vector3::Dot_Product((Vector3 &)(prevXForm->operator [](0)),(Vector3 &)(objectToWorld.operator [](0))));
	if (cosAngle >= cosAngleToCare)
	{	cosAngle = fabs (Vector3::Dot_Product((Vector3 &)(prevXForm->operator [](1)),(Vector3 &)(objectToWorld.operator [](1))));
		if (cosAngle >= cosAngleToCare)
		{
			cosAngle = fabs (Vector3::Dot_Product((Vector3 &)(prevXForm->operator [](2)),(Vector3 &)(objectToWorld.operator [](2))));
			if (cosAngle < cosAngleToCare)
				isMeshRotating=true;
		}
		else
			isMeshRotating =true;
	}
	else
		isMeshRotating =true;
#endif	//near light source
#endif // CNC3

	// get the light
	lightPosWorld = TheW3DShadowManager->getLightPosWorld(lightIndex);

	// get the object
	meshXform->Get_Translation(&objectCenter);	//current mesh position

	// check if object has a limit/clamp on shadow length and adjust light
	// position of necessary.
	if (m_shadowLengthScale)
	{	//Find light's distance from origin in xy plane
		Real lightXYDistance = sqrt(lightPosWorld.X*lightPosWorld.X + lightPosWorld.Y * lightPosWorld.Y);
		Real newZ=lightXYDistance*m_shadowLengthScale;

		if (newZ > lightPosWorld.Z)
		{	//clamped z component is higher than actual light position allows so adjust it.
			lightPosWorld.Z = newZ;
		}
	}

	if (lightPosWorld != m_lightPosHistory[ lightIndex ][meshIndex])
	{	//Light position has moved, see if enough to matter

		// compute vector from the light to the current object position
		toLight = objectCenter - lightPosWorld;
		toLight.Normalize();

		// compute vector from the previous light to the object position
		toPrevLight = objectCenter - m_lightPosHistory[ lightIndex ][meshIndex];
		toPrevLight.Normalize();

		Real cosAngle = fabs (Vector3::Dot_Product(toLight,toPrevLight));
		if (cosAngle < cosAngleToCare)	//less than 45 degree change
			isLightMoving =true;
	}
	else
	///@todo: Find a better way to deal with this - use maximum extrusion once!  Also avoid hit for units climbing hills.
	if (fabs(objectCenter.Z - prevXForm->operator [](2).W) > SHADOW_EXTRUSION_BUFFER)
		isLightMoving = true;	//treat model rising just like rotation since volume needs update for longer extrusion.

	// reconstruct if needed
	if (isLightMoving || isMeshRotating)
	{
		//
		// transform the light in the world to object space, we
		// care only about the rotation of components for the coordinate
		// system change, not the translations
		//
		Real det;
		Matrix4x4::Inverse(&worldToObject, &det, &objectToWorld);

		// find out light position in object space
		Matrix4x4::Transform_Vector(worldToObject,lightPosWorld,&lightPosObject);

		//Updating shadow volumes is expensive, so verify that this volume is even visible.

		//Generate bounding box around shadow volume by extruding AABB corners
		AABoxClass box(meshBox);	//copy current mesh bounding box (will be smaller than shadow box).
		SphereClass sphere;			//rough bounding sphere of shadow volume - based on box.
		Vector3 Corners[8];
		Vector3 lightRay;
		Real vectorScale,vectorScaleTemp, vectorScaleMax;
		Real length;

		//Get vertices of top of bounding box
		Corners[0]=box.Center+box.Extent;	//top right corner
		Corners[1]=Corners[0];
		Corners[1].X -= 2.0f*box.Extent.X;		//top left corner
		Corners[2]=Corners[1];
		Corners[2].Y -= 2.0f*box.Extent.Y;		//bottom left corner
		Corners[3]=Corners[2];
		Corners[3].X += 2.0f*box.Extent.X;		//bottom right corner

		//Project top volume corners onto ground plane
		lightRay = Corners[0] - lightPosWorld;	//vector light to corner
		length= 1.0f/lightRay.Length();
		lightRay *= length;
		vectorScaleMax=vectorScale=(Real)fabs((Corners[0].Z-floorZ)/lightRay.Z);	//length of vector from top corner to ground.
		Corners[4]=Corners[0]+lightRay*vectorScale;
		vectorScaleMax *= length;

		lightRay = Corners[1] - lightPosWorld;	//vector light to corner
		length= 1.0f/lightRay.Length();
		lightRay *= length;
		vectorScaleTemp=(Real)fabs((Corners[1].Z-floorZ)/lightRay.Z);	//length of vector from top corner to ground.
		Corners[5]=Corners[1]+lightRay*vectorScaleTemp;
		vectorScaleTemp *= length;

		if (vectorScaleTemp > vectorScaleMax)
			vectorScaleMax=vectorScaleTemp;	//keep track of maximum required extrusion length.

		lightRay = Corners[2] - lightPosWorld;	//vector light to corner
		length= 1.0f/lightRay.Length();
		lightRay *= length;
		vectorScale=(Real)fabs((Corners[2].Z-floorZ)/lightRay.Z);	//length of vector from top corner to ground.
		Corners[6]=Corners[2]+lightRay*vectorScale;
		vectorScale *= length;

		if (vectorScale > vectorScaleMax)
			vectorScaleMax=vectorScale;	//keep track of maximum required extrusion length.

		lightRay = Corners[3] - lightPosWorld;	//vector light to corner
		length= 1.0f/lightRay.Length();
		lightRay *= length;
		vectorScaleTemp=(Real)fabs((Corners[3].Z-floorZ)/lightRay.Z);	//length of vector from top corner to ground.
		Corners[7]=Corners[3]+lightRay*vectorScaleTemp;
		vectorScaleTemp *= length;

		if (vectorScaleTemp > vectorScaleMax)
			vectorScaleMax=vectorScaleTemp;	//keep track of maximum required extrusion length.

		box.Init(Corners, 8);	//generate a new bounding box
		sphere.Init(box.Center,box.Extent.Length());	//generate object space bounding sphere containing box.

		CollisionMath::OverlapType result=CollisionMath::Overlap_Test(*shadowCameraFrustum,sphere);
		if (result == CollisionMath::OVERLAPPED)	//do a more accurate test
			result=CollisionMath::Overlap_Test(*shadowCameraFrustum, box);

		if (result != CollisionMath::OUTSIDE)
		{
			//
			// reset the silhouette data and build a new one from this light
			// source perspective
			//

			if (m_numSilhouetteIndices[meshIndex] != 0)
			{	//this silhouette was built before and is being updated.
				//this probably means it will change again in the future.
				//make future updates faster by pre-caching face normals.
				m_geometry->getMesh(meshIndex)->buildPolygonNormals();
			}
			resetSilhouette(meshIndex);
			buildSilhouette(meshIndex, &lightPosObject);

			//
			// in a multiple shadow situation we would be allocating a volume
			// for this current shadow light, not the 0 index volume all the time
			//
			if (!m_shadowVolume[ lightIndex ][meshIndex])
				allocateShadowVolume( lightIndex,meshIndex );
			if( m_shadowVolumeVB[ lightIndex ][meshIndex] )
			{	//Updating an existing vertex buffer shadow volume.  This means we're
				//probably dealing with an animated mesh.  Update flags to reflect this fact.
				if (isMeshRotating || isLightMoving)
				{
					if (isMeshRotating)
					{	//rotating meshes will most likely need updates each frame, so stop using static vertex buffers.
						m_shadowVolume[ lightIndex ][meshIndex]->SetFlags(
							m_shadowVolume[ lightIndex ][meshIndex]->GetFlags() | SHADOW_DYNAMIC);
					}
					//release memory used to store vertices/polygons
					resetShadowVolume( lightIndex,meshIndex );	//free vertex buffers since not used for dynamic.
					//Resize the shadow volume since we'll need room to store the vertices in memory instead of VB.
					allocateShadowVolume( lightIndex,meshIndex );
				}
			}

			//
			// construct the shadow volume at this light position in the
			// passed shadow volume geometry index
			//
			// GeneralsX @bugfix BenderAI 21/03/2026 Clamp extrusion length to prevent runaway volume spikes on shallow light rays.
			if (vectorScaleMax > MAX_EXTRUSION_LENGTH)
				vectorScaleMax = MAX_EXTRUSION_LENGTH;
			if (m_shadowVolume[ lightIndex ][meshIndex]->GetFlags() & SHADOW_DYNAMIC)
				constructVolume( &lightPosObject, vectorScaleMax, lightIndex, meshIndex );
			else
				constructVolumeVB( &lightPosObject, vectorScaleMax, lightIndex, meshIndex );

			//
			// store the current light position and orientation that
			// we constructed shadow info at
			//
			m_objectXformHistory[ lightIndex ][meshIndex] = objectToWorld;
			m_lightPosHistory[lightIndex][meshIndex] = lightPosWorld;

			box.Translate(-objectCenter);	//translate box to object space.
			m_shadowVolume[ lightIndex ][meshIndex]->setBoundingBox(box);
			sphere.Center -= objectCenter;
			m_shadowVolume[ lightIndex ][meshIndex]->setBoundingSphere(sphere);
			m_shadowVolume[ lightIndex ][meshIndex]->setVisibleState(Geometry::STATE_VISIBLE);	//this volume needs rendering.
		}
		else
		if (m_shadowVolume[ lightIndex ][meshIndex])
		{	//outside view frustum, shadow wasn't updated.
			box.Translate(-objectCenter);	//translate box to object space.
			m_shadowVolume[ lightIndex ][meshIndex]->setBoundingBox(box);
			sphere.Center -= objectCenter;
			m_shadowVolume[ lightIndex ][meshIndex]->setBoundingSphere(sphere);
			m_shadowVolume[ lightIndex ][meshIndex]->setVisibleState(Geometry::STATE_INVISIBLE);
		}
	}
	else
	{	//not reconstructing volume, so don't know if visible or not.
		if (m_shadowVolume[ lightIndex ][meshIndex])
			m_shadowVolume[ lightIndex ][meshIndex]->setVisibleState(Geometry::STATE_UNKNOWN);
	}
}

// addSilhouetteEdge ==========================================================
// It has been determined that the polygon neighbor in the "neighborIndex"
// of "visible" needs to be added to the silhouette.  We will add those two
// vertex indices to the silhouette in the order they were specified in
// "visible" to assure that the constructed edge is in counter clockwise order
// ============================================================================
void W3DVolumetricShadow::addSilhouetteEdge(Int meshIndex, PolyNeighbor *visible, PolyNeighbor *hidden )
{
	Int i;
	Int neighborIndex = 0;
	Short visibleIndexList[ 3 ];
	Short edgeStart, edgeEnd;

	W3DShadowGeometryMesh *geomMesh=m_geometry->getMesh(meshIndex);

	// sanity
	assert( visible && hidden );

	//
	// which index in the neighbor list of "visible" refers to the
	// polygon "hidden"
	//
	for( i = 0; i < MAX_POLYGON_NEIGHBORS; i++ )
	{

		if( visible->neighbor[ i ].neighborIndex == hidden->myIndex )
		{

			neighborIndex = i;
			break;  // exit for

		}

	}

	// get the three vertex indices of "visible"
	geomMesh->GetPolygonIndex( visible->myIndex, visibleIndexList );

	//
	// we know that 2 of the 3 vertex indices will be present in the edge.
	// will construct the edge as follows to ensure we have counter
	// clockwise order.  note that this assumes the vertices of the
	// polygons specified in the geometry are in counter clockwise order,
	// which they are
	//
	// 1) [ v1  Absent, v2 Present, v3 Present ] -> edge = (v2, v3)
	// 2) [ v1 Present, v2  Absent, v3 Present ] -> edge = (v3, v1)
	// 3) [ v1 Present, v2 Present, v3 Absent  ] -> edge = (v1, v2)
	//
	if( (visibleIndexList[ 0 ] !=
			 visible->neighbor[ neighborIndex ].neighborEdgeIndex[ 0 ]) &&
			(visibleIndexList[ 0 ] !=
			visible->neighbor[ neighborIndex ].neighborEdgeIndex[ 1 ]) )
	{

		// case 1 above
		edgeStart = visibleIndexList[ 1 ];
		edgeEnd = visibleIndexList[ 2 ];

	}
	else if( (visibleIndexList[ 1 ] !=
					 visible->neighbor[ neighborIndex ].neighborEdgeIndex[ 0 ]) &&
					 (visibleIndexList[ 1 ] !=
					 visible->neighbor[ neighborIndex ].neighborEdgeIndex[ 1 ]) )
	{

		// case 2 above
		edgeStart = visibleIndexList[ 2 ];
		edgeEnd = visibleIndexList[ 0 ];

	}
	else
	{

		// case 3 above
		edgeStart = visibleIndexList[ 0 ];
		edgeEnd = visibleIndexList[ 1 ];

	}

	// add to silhouette edge list
	addSilhouetteIndices(meshIndex, edgeStart, edgeEnd );

}

// addNeighborlessEdges =======================================================
// Given a polygon neighbor information, it has been determined that this
// polygon is visible and has edges which are not connected to other
// polygons, these edges need to be added to the silhouette.  The edge(s)
// must be added in such an order that we create silhouette edges in a
// counter clockwise order.
// ============================================================================
void W3DVolumetricShadow::addNeighborlessEdges(Int meshIndex, PolyNeighbor *us )
{
	Short vertexIndexList[ 3 ];
	Int i, j;
	Short edgeStart, edgeEnd;
	Bool addEdge;

	// sanity
	assert( us );

	W3DShadowGeometryMesh *geomMesh = m_geometry->getMesh(meshIndex);

	// get the vertex index list from the geometry
	geomMesh->GetPolygonIndex( us->myIndex, vertexIndexList );

	//
	// go through each edge, if these indices to NOT appear TOGETHER in
	// neighbor list then we must add it.
	//
	for( i = 0; i < 3; i++ )
	{

		// get the edge start and end vertex indices
		edgeStart = vertexIndexList[ i ];
		if( i == 2 )
			edgeEnd = vertexIndexList[ 0 ];  // wraps to begging of list
		else
			edgeEnd = vertexIndexList[ i + 1 ];

		// do these two vertices appear in a neighbor list of the poly?
		addEdge = TRUE;
		for( j = 0; j < MAX_POLYGON_NEIGHBORS; j++ )
		{

			if( us->neighbor[ j ].neighborIndex != NO_NEIGHBOR )
			{

				if( (us->neighbor[ j ].neighborEdgeIndex[ 0 ] == edgeStart &&
						 us->neighbor[ j ].neighborEdgeIndex[ 1 ] == edgeEnd) ||
						(us->neighbor[ j ].neighborEdgeIndex[ 1 ] == edgeStart &&
						 us->neighbor[ j ].neighborEdgeIndex[ 0 ] == edgeEnd) )
				{

					addEdge = FALSE;
					break;  // exit for j, no need to search on

				}

			}

		}

		// add the edge if no neighbors have that edge
		if( addEdge == TRUE )
		{

			addSilhouetteIndices(meshIndex, edgeStart, edgeEnd );

		}

	}

}

// addSilhouetteIndices =======================================================
// Add these two indices to the silhouette data
// ============================================================================
void W3DVolumetricShadow::addSilhouetteIndices(Int meshIndex, Short edgeStart, Short edgeEnd )
{

//	DBGPRINTF(( "addSilhouetteIndices: Adding (%d,%d), the storage before the add is = (%d/%d)\n",
//							edgeStart, edgeEnd, m_numSilhouetteIndices, m_maxSilhouetteEntries ));

	// add to silhouette edge list
	assert( m_numSilhouetteIndices[meshIndex] < m_maxSilhouetteEntries[meshIndex] );
	m_silhouetteIndex[meshIndex][ m_numSilhouetteIndices[meshIndex]++ ] = edgeStart;
	assert( m_numSilhouetteIndices[meshIndex] < m_maxSilhouetteEntries[meshIndex] );
	m_silhouetteIndex[meshIndex][ m_numSilhouetteIndices[meshIndex]++ ] = edgeEnd;

}

// buildSilhouette ============================================================
// Given a light position, and our polygon neighbor information this will
// build the silhouette of the object edges from the given light position
// ============================================================================
void W3DVolumetricShadow::buildSilhouette(Int meshIndex, Vector3 *lightPosObject)
{
	PolyNeighbor *polyNeighbor;  // the poly we're looking at right now
	Vector3 lightVector;  // vector from light to polygon
	Bool visibleNeighborless;
	Int numPolys;  // number of polys in our geometry
	W3DShadowGeometryMesh *geomMesh;
	Int i, j;
	Int meshEdgeStart=0; //index to first edge contributed by specific mesh

	//
	// go through each of our shadow geometry polygon info and find out
	// which polys are visible from this light source and which ones are not
	//

	geomMesh = m_geometry->getMesh(meshIndex);

	//record where this meshes indices will begin.
	meshEdgeStart=m_numSilhouetteIndices[meshIndex];

	numPolys = geomMesh->GetNumPolygon();
	for( i = 0; i < numPolys; i++ )
	{
		Short poly[ 3 ];

		// get this polygon neighbor information
		polyNeighbor = geomMesh->GetPolyNeighbor( i );

		// take this opportunity to initialize our processing flags to zero
		polyNeighbor->status = 0;

		// get the normal for this polygon
		const Vector3& normal=geomMesh->GetPolygonNormal(i);

		// get the vertex indices at this polygon
		geomMesh->GetPolygonIndex( i, poly );

		//
		// find out "lightVector" to this polygon
		//
		// since our light source could be very close to the object and that
		// would change the shadow we are going to say that the light vector
		// is from the light position to one of the vertices in the polygon.
		// To be more correct we should use the center of the polygon but
		// this is a good approximation ... an ever broader approximation that
		// we could use would be the object center
		//
		const Vector3& vertex=geomMesh->GetVertex( poly[ 0 ] );
		lightVector= vertex - *lightPosObject;

		//
		// dot the light vector with the normal of the polygon to see if the
		// poly is visible from this location
		//
		if( Vector3::Dot_Product( lightVector, normal ) < 0.0f )
			BitSet( polyNeighbor->status, POLY_VISIBLE );

	}

	//
	// check all our polys using our poly neighbors, where one poly neighbor
	// is not the same visible status as a neighbor that represents a
	// silhouette edge
	//
	for( i = 0; i < numPolys; i++ )
	{
		PolyNeighbor *otherNeighbor;

		// get this poly neighbor ... this is "us"
		polyNeighbor = geomMesh->GetPolyNeighbor( i );

		// initialize ourselves to not be a visible edge
		visibleNeighborless = FALSE;

		// check our 3 potential neighbors
		for( j = 0; j < MAX_POLYGON_NEIGHBORS; j++ )
		{

			// initialize this neighbor to nuttin
			otherNeighbor = nullptr;

			// get our neighbor if present and cull them if processed
			if( polyNeighbor->neighbor[ j ].neighborIndex != NO_NEIGHBOR )
			{

				// get the jth polygon neighbor ... this is "them"
				otherNeighbor =
					geomMesh->GetPolyNeighbor(
						polyNeighbor->neighbor[ j ].neighborIndex );

				//
				// ignore neighbors that are marked as processed as those
				// ones have already detected edges if present
				//
				if( BitIsSet( otherNeighbor->status, POLY_PROCESSED ) )
					continue;  // for j

			}

			//
			// finally, if our own visible status is different from our
			// neighbor visible status then that defines an edge we must
			// add to the silhouette.  Also, a visible polygon that has
			// no neighbor automatically makes a silhouette edge.  Note that
			// if we have no neighbor we just record the fact that we have
			// real model end edges to add after this inner j loop;
			//
			if( BitIsSet( polyNeighbor->status, POLY_VISIBLE ) )
			{

				// check for no neighbor edges
				if( otherNeighbor == nullptr )
				{

					visibleNeighborless = TRUE;

				}
				else if( BitIsSet( otherNeighbor->status, POLY_VISIBLE ) == FALSE )
				{

					// "we" are visible and "they" are not
					addSilhouetteEdge(meshIndex, polyNeighbor, otherNeighbor );

				}

			}
			else if( otherNeighbor != nullptr &&
							 BitIsSet( otherNeighbor->status, POLY_VISIBLE ) )
			{

				// "they" are visible and "we" are not
				addSilhouetteEdge(meshIndex, otherNeighbor, polyNeighbor );

			}

		}

		//
		// if this polygon is visible, add any edges that are not
		// neighbors of adjacent polygons.
		//
		if( visibleNeighborless == TRUE )
		{

			addNeighborlessEdges(meshIndex, polyNeighbor );

		}

		//
		// this polyNeighbor is now considered "processed", any other
		// polygons that reference back to this one can ignore their
		// processing cause any edges were already detected
		//
		BitSet( polyNeighbor->status, POLY_PROCESSED );

	}

	//record number of edge indices contrinuted by this mesh
	m_numIndicesPerMesh[meshIndex]=m_numSilhouetteIndices[meshIndex]-meshEdgeStart;

}

// constructVolume ============================================================
// Given a fresh new geometry class called "shadowVolume" to hold the actual
// shadow volume data, this method will create the shadow volume polygons
// given the information in the current silhouette of this Shadow and the
// light source position.
//
// The light source should be in object space and the shadow volume polygon
// data is also constructed in object space.
//
// The polygon we will create for a given edge will be that edge extruded
// out in the direction away from the light source.  This conceptual 4 sided
// polygon is however broken up into two triangles for storage.
//
// This version is designed to construct the volume inside a system memory
// buffer - to be rendered via a dynamic vertex buffer.
//
// ============================================================================
void W3DVolumetricShadow::constructVolume( Vector3 *lightPosObject,Real shadowExtrudeDistance, Int volumeIndex, Int meshIndex )
{
	Geometry *shadowVolume;
	Vector3 extrude2;  // the polypoints extruded from edge and light
	Short indexList[ 3 ];
	Int i,k;
	Int vertexCount;
	Int polygonCount;
	Int indicesPerMesh;
	W3DShadowGeometryMesh *geomMesh;

	// sanity
	if( volumeIndex < 0 ||
			volumeIndex >= MAX_SHADOW_LIGHTS ||
			lightPosObject == nullptr )
	{

		assert( 0 );
		return;

	}

	// get the geometry struct we're storing the actual shadow volume data in
	shadowVolume = m_shadowVolume[ volumeIndex ][meshIndex];

	if( shadowVolume == nullptr )
	{

//		DBGPRINTF(( "No volume allocated at index '%d'\n", volumeIndex ));
		assert( 0 );
		return;

	}

	// step through each of the silhouette pairs
	vertexCount = 0;
	polygonCount = 0;

	indicesPerMesh=m_numIndicesPerMesh[meshIndex];
	if (!indicesPerMesh)
		return;	//nothing to draw

	geomMesh = m_geometry->getMesh(meshIndex);

	shadowVolume->SetNumActivePolygon(0);
	shadowVolume->SetNumActiveVertex(0);

#ifdef RECORD_SHADOW_STRIP_STATS
	Int numStrips=0;	//keeps track of number of strips generated.
	Int stripLength=1;	//keeps track of segments in strip (each being 2 triangles).
	Int maxStripLength=0;	//keeps track of longest strip generated.
#endif

	Short *silhouetteIndices=m_silhouetteIndex[meshIndex];

	//Initialize first strip info
	Short stripStartIndex=silhouetteIndices[ 0 ];
	Short stripStartVertex=0;

	//Insert the first vertex and extrusion into strip.
	// get edge point
	const Vector3& ev2=  // second edge of silhouette
		geomMesh->GetVertex( silhouetteIndices[ 0 ] );

	// take one edge point and extrude away from the light
	extrude2 = ev2 - *lightPosObject;
	extrude2 *= shadowExtrudeDistance;
	extrude2 += ev2;

	shadowVolume->SetVertex( vertexCount, &ev2 );
	shadowVolume->SetVertex( vertexCount + 1, &extrude2 );

	vertexCount=2;
	Int lastEdgeVertex2Index=0;
	Int lastExtrude2Index=1;

	for( i = 0; i < indicesPerMesh; i += 2 )
	{
		Short currentEdgeEnd=silhouetteIndices[i+1];

		//look for edge connected to this one and move it next to this edge
		//in index list.  Rendering edges back-to-back improves vertex cache usage.
		for (k=i+2; k<indicesPerMesh; k+=2)
			if (silhouetteIndices[k]==currentEdgeEnd)
			{	//swap the two edges
				// GeneralsX @bugfix BenderAI 21/03/2026 Avoid strict-aliasing/alignment UB when swapping edge pairs on 64-bit/ARM.
				Short edge0a = silhouetteIndices[i+2];
				Short edge0b = silhouetteIndices[i+3];
				silhouetteIndices[i+2] = silhouetteIndices[k];
				silhouetteIndices[i+3] = silhouetteIndices[k+1];
				silhouetteIndices[k] = edge0a;
				silhouetteIndices[k+1] = edge0b;
				break;
			}

		if (k >= indicesPerMesh)
		{	//reached end of strip. Insert final edge.
			//Check if last edge wraps around to start.
			if (currentEdgeEnd == stripStartIndex)
			{	//add end of strip that wraps around to start (forming closed cylinder/shape)
				//
				// add the polygon consisting of the two edge vertices and the
				// first extruded point
				//
				indexList[ 0 ] = lastEdgeVertex2Index;  // lastedgeVertex2 index
				indexList[ 1 ] = lastExtrude2Index;  // lastextrude2 index
				indexList[ 2 ] = stripStartVertex;  // edgeVertex2 index
				shadowVolume->SetPolygonIndex( polygonCount, indexList );

				indexList[ 0 ] = stripStartVertex;  // edgeVertex2 index
				indexList[ 1 ] = lastExtrude2Index;  // extrude1 index
				indexList[ 2 ] = stripStartVertex+1;  // extrude2 index
				shadowVolume->SetPolygonIndex( polygonCount + 1, indexList );
			}
			else
			{	//add end of strip.  Finishes the last 2 polygons.
				const Vector3& ev=geomMesh->GetVertex(currentEdgeEnd);
				shadowVolume->SetVertex( vertexCount, &ev );

				//
				// add the polygon consisting of the two edge vertices and the
				// first extruded point
				//
				indexList[ 0 ] = lastEdgeVertex2Index;  // lastedgeVertex2 index
				indexList[ 1 ] = lastExtrude2Index;  // lastextrude2 index
				indexList[ 2 ] = vertexCount;  // edgeVertex2 index
				shadowVolume->SetPolygonIndex( polygonCount, indexList );

				// take the other edge point and extrude away from light
				extrude2 = ev - *lightPosObject;
				extrude2 *= shadowExtrudeDistance;
				extrude2 += ev;
				// add the one new vertex
				shadowVolume->SetVertex( vertexCount + 1, &extrude2 );

				indexList[ 0 ] = vertexCount;  // edgeVertex2 index
				indexList[ 1 ] = lastExtrude2Index;  // extrude1 index
				indexList[ 2 ] = vertexCount+1;  // extrude2 index
				shadowVolume->SetPolygonIndex( polygonCount + 1, indexList );

				lastEdgeVertex2Index=vertexCount;
				lastExtrude2Index=vertexCount+1;
				vertexCount += 2;
			}

			if ((i+2) >= indicesPerMesh)
			{	//finished with all silhouette edges
				polygonCount += 2;
#ifdef RECORD_SHADOW_STRIP_STATS
				numStrips++;
#endif
				break;	//reached end of all edges
			}

			//Start a new strip by adding first vertex and extrusion.
			const Vector3& ev=geomMesh->GetVertex( silhouetteIndices[ i+2 ] );
			// take one edge point and extrude away from the light
			extrude2 = ev - *lightPosObject;
			extrude2 *= shadowExtrudeDistance;
			extrude2 += ev;

			lastEdgeVertex2Index=vertexCount;
			lastExtrude2Index=vertexCount + 1;
			//record start of new strip info
			stripStartIndex=silhouetteIndices[ i+2 ];
			stripStartVertex=lastEdgeVertex2Index;

			shadowVolume->SetVertex( lastEdgeVertex2Index, &ev );
			shadowVolume->SetVertex( lastExtrude2Index, &extrude2 );
			vertexCount += 2;

			polygonCount += 2;
#ifdef RECORD_SHADOW_STRIP_STATS
			numStrips++;
			stripLength=1;
#endif
			continue;
		}
		else
		{	//continue existing strip by adding extra vertex and extrusion

			const Vector3& ev=geomMesh->GetVertex( currentEdgeEnd );
			shadowVolume->SetVertex( vertexCount, &ev );
			//
			// add the polygon consisting of the two edge vertices and the
			// first extruded point
			//
			indexList[ 0 ] = lastEdgeVertex2Index;  // lastedgeVertex2 index
			indexList[ 1 ] = lastExtrude2Index;  // lastextrude2 index
			indexList[ 2 ] = vertexCount;  // edgeVertex2 index
			shadowVolume->SetPolygonIndex( polygonCount, indexList );

			// take the other edge point and extrude away from light
			extrude2 = ev - *lightPosObject;
			extrude2 *= shadowExtrudeDistance;
			extrude2 += ev;

			// add the one new vertex
			shadowVolume->SetVertex( vertexCount + 1, &extrude2 );

			indexList[ 0 ] = vertexCount;  // edgeVertex2 index
			indexList[ 1 ] = lastExtrude2Index;  // extrude1 index
			indexList[ 2 ] = vertexCount+1;  // extrude2 index
			shadowVolume->SetPolygonIndex( polygonCount + 1, indexList );

			lastEdgeVertex2Index=vertexCount;
			lastExtrude2Index=vertexCount+1;

			vertexCount += 2;
			polygonCount += 2;
		}
#ifdef RECORD_SHADOW_STRIP_STATS
		//Continuing strip.
		stripLength++;
		// GeneralsX @bugfix BenderAI 13/02/2026 Use MAX macro (cross-platform, defined in BaseTypeCore.h)
		maxStripLength=MAX(maxStripLength,stripLength);
#endif
	}

	shadowVolume->SetNumActivePolygon(polygonCount);
	shadowVolume->SetNumActiveVertex(vertexCount);
}

// constructVolumeVB ==========================================================
// Given a fresh new geometry class called "shadowVolume" to hold the actual
// shadow volume data, this method will create the shadow volume polygons
// given the information in the current silhouette of this Shadow and the
// light source position.
//
// The light source should be in object space and the shadow volume polygon
// data is also constructed in object space.
//
// The polygon we will create for a given edge will be that edge extruded
// out in the direction away from the light source.  This conceptual 4 sided
// polygon is however broken up into two triangles for storage.
//
// This version is designed to construct the volume directly inside a vertex
// buffer so it's optimal for static geometry.  Since it's assumed to be called
// only once per model, we can use some more expensive computations to generate
// the volume.
//
// ============================================================================
void W3DVolumetricShadow::constructVolumeVB( Vector3 *lightPosObject,Real shadowExtrudeDistance, Int volumeIndex, Int meshIndex )
{
	Geometry *shadowVolume;
	Vector3 extrude2;  // the polypoints extruded from edge and light
	Vector3 edgeVertex2;  // second edge of silhouette
	Int i,k;
	Int vertexCount;
	Int polygonCount;
	Int indicesPerMesh;
	W3DShadowGeometryMesh *geomMesh;

	W3DBufferManager::W3DVertexBufferSlot *vbSlot;
	W3DBufferManager::W3DIndexBufferSlot *ibSlot;

	// sanity
	if( volumeIndex < 0 ||
			volumeIndex >= MAX_SHADOW_LIGHTS ||
			lightPosObject == nullptr )
	{

		assert( 0 );
		return;

	}

	// get the geometry struct we're storing the actual shadow volume data in
	shadowVolume = m_shadowVolume[ volumeIndex ][meshIndex];

	if( shadowVolume == nullptr )
	{

//		DBGPRINTF(( "No volume allocated at index '%d'\n", volumeIndex ));
		assert( 0 );
		return;

	}

	//*****************************************************************************************/
	//Do an initial pass through silhouette data to determine the actual vertex/polygon counts.
	//This number can't be determined any other way since it depends on degree of vertex sharing
	//in model.  We don't want to overallocate because vertex buffer space is limited.
	//This pass is also used to sort the edges so they are all connected in strip order.
	{
		#ifdef RECORD_SHADOW_STRIP_STATS
			Int numStrips=0;	//keeps track of number of strips generated.
			Int stripLength=1;	//keeps track of segments in strip (each being 2 triangles).
			Int maxStripLength=0;	//keeps track of longest strip generated.
		#endif

		// step through each of the silhouette pairs
		vertexCount = 0;
		polygonCount = 0;

		indicesPerMesh=m_numIndicesPerMesh[meshIndex];
		if (!indicesPerMesh)
			return;	//nothing to draw

		Short *silhouetteIndices=m_silhouetteIndex[meshIndex];

		//Initialize first strip info
		Short stripStartIndex=silhouetteIndices[ 0 ];
		Short stripStartVertex=0;

		vertexCount=2;
		Int lastEdgeVertex2Index=0;
		Int lastExtrude2Index=1;

		for( i = 0; i < indicesPerMesh; i += 2 )
		{
			Short currentEdgeEnd=silhouetteIndices[i+1];

			//look for edge connected to this one and move it next to this edge
			//in index list.  Rendering edges back-to-back improves vertex cache usage.
			for (k=i+2; k<indicesPerMesh; k+=2)
				if (silhouetteIndices[k]==currentEdgeEnd)
				{	//swap the two edges
					// GeneralsX @bugfix BenderAI 21/03/2026 Avoid strict-aliasing/alignment UB when swapping edge pairs on 64-bit/ARM.
					Short edge0a = silhouetteIndices[i+2];
					Short edge0b = silhouetteIndices[i+3];
					silhouetteIndices[i+2] = silhouetteIndices[k];
					silhouetteIndices[i+3] = silhouetteIndices[k+1];
					silhouetteIndices[k] = edge0a;
					silhouetteIndices[k+1] = edge0b;
					break;
				}

			if (k >= indicesPerMesh)
			{	//reached end of strip. Insert final edge.
				//Check if last edge wraps around to start.
				if (currentEdgeEnd == stripStartIndex)
				{	//add end of strip that wraps around to start (forming closed cylinder/shape)
					//
					// add the polygon consisting of the two edge vertices and the
					// first extruded point
					//
				}
				else
				{	//add end of strip.  Finishes the last 2 polygons.
					lastEdgeVertex2Index=vertexCount;
					lastExtrude2Index=vertexCount+1;
					vertexCount += 2;
				}

				if ((i+2) >= indicesPerMesh)
				{	//finished with all silhouette edges
					polygonCount += 2;
	#ifdef RECORD_SHADOW_STRIP_STATS
					numStrips++;
	#endif
					break;	//reached end of all edges
				}

				lastEdgeVertex2Index=vertexCount;
				lastExtrude2Index=vertexCount + 1;
				//record start of new strip info
				stripStartIndex=silhouetteIndices[ i+2 ];
				stripStartVertex=lastEdgeVertex2Index;

				vertexCount += 2;

				polygonCount += 2;
	#ifdef RECORD_SHADOW_STRIP_STATS
				numStrips++;
				stripLength=1;
	#endif
				continue;
			}
			else
			{	//continue existing strip by adding extra vertex and extrusion

				lastEdgeVertex2Index=vertexCount;
				lastExtrude2Index=vertexCount+1;

				vertexCount += 2;
				polygonCount += 2;
			}
	#ifdef RECORD_SHADOW_STRIP_STATS
			//Continuing strip.
			stripLength++;
			// GeneralsX @bugfix BenderAI 13/02/2026 Use MAX macro (cross-platform)
			maxStripLength=MAX(maxStripLength,stripLength);
	#endif
		}
	}
	//***********************************************************************************************

	DEBUG_ASSERTCRASH(m_shadowVolumeVB[ volumeIndex ][meshIndex] == nullptr,("Updating Existing Static Vertex Buffer Shadow"));
	vbSlot=m_shadowVolumeVB[ volumeIndex ][meshIndex] = TheW3DBufferManager->getSlot(W3DBufferManager::VBM_FVF_XYZ,
		vertexCount);

	DEBUG_ASSERTCRASH(vbSlot != nullptr, ("Can't allocate vertex buffer slot for shadow volume"));
	if (vbSlot != nullptr)
	{
		DEBUG_ASSERTCRASH(vbSlot->m_size >= vertexCount,("Overflowing Shadow Vertex Buffer Slot"));
	}

	DEBUG_ASSERTCRASH(m_shadowVolume[ volumeIndex ][meshIndex]->GetNumPolygon() == 0,("Updating Existing Static Shadow Volume"));

	DEBUG_ASSERTCRASH(m_shadowVolumeIB[ volumeIndex ][meshIndex] == nullptr,("Updating Existing Static Index Buffer Shadow"));
	ibSlot=m_shadowVolumeIB[ volumeIndex ][meshIndex] = TheW3DBufferManager->getSlot(polygonCount*3);

	DEBUG_ASSERTCRASH(ibSlot != nullptr, ("Can't allocate index buffer slot for shadow volume"));
	if (ibSlot != nullptr)
	{
		DEBUG_ASSERTCRASH(ibSlot->m_size >= (polygonCount*3),("Overflowing Shadow Index Buffer Slot"));
	}

	if (!ibSlot || !vbSlot)
	{	//could not allocate storage to hold buffers
		if (ibSlot)
			TheW3DBufferManager->releaseSlot(ibSlot);
		if (vbSlot)
			TheW3DBufferManager->releaseSlot(vbSlot);

		m_shadowVolumeIB[ volumeIndex ][meshIndex]=nullptr;
		m_shadowVolumeVB[ volumeIndex ][meshIndex]=nullptr;
		return;
	}

	geomMesh = m_geometry->getMesh(meshIndex);

	DX8VertexBufferClass::AppendLockClass lockVtxBuffer(vbSlot->m_VB->m_DX8VertexBuffer,vbSlot->m_start,vertexCount);
	VertexFormatXYZ *vb = (VertexFormatXYZ*)lockVtxBuffer.Get_Vertex_Array();

	if (vb == nullptr)
		return;

	DX8IndexBufferClass::AppendLockClass lockIdxBuffer(ibSlot->m_IB->m_DX8IndexBuffer,ibSlot->m_start,polygonCount*3);
	UnsignedShort *ib = (UnsignedShort*)lockIdxBuffer.Get_Index_Array();

	if (ib == nullptr)
		return;

	shadowVolume->SetNumActivePolygon(polygonCount);
	shadowVolume->SetNumActiveVertex(vertexCount);

	Short *silhouetteIndices=m_silhouetteIndex[meshIndex];

	//Initialize first strip info
	Short stripStartIndex=silhouetteIndices[ 0 ];
	Short stripStartVertex=0;

	//Insert the first vertex and extrusion into strip.
	// get edge point
	const Vector3& ev=geomMesh->GetVertex( silhouetteIndices[ 0 ] );

	// take one edge point and extrude away from the light
	extrude2 = ev - *lightPosObject;
	extrude2 *= shadowExtrudeDistance;
	extrude2 += ev;

	*vb++ = *(VertexFormatXYZ *)&ev;
	*vb++ = *(VertexFormatXYZ *)&extrude2;

	vertexCount=2;
	polygonCount=0;
	Int lastEdgeVertex2Index=0;
	Int lastExtrude2Index=1;

	for( i = 0; i < indicesPerMesh; i += 2 )
	{
		Short currentEdgeEnd=silhouetteIndices[i+1];

		//Check if another edge is connected to this one, edges were sorted in initial
		//pass so only need to check the next one.
		if (((i+2) >= indicesPerMesh) || silhouetteIndices[i+2] != currentEdgeEnd)
		{	//reached end of strip. Insert final edge.
			//Check if last edge wraps around to start.
			if (currentEdgeEnd == stripStartIndex)
			{	//add end of strip that wraps around to start (forming closed cylinder/shape)
				//
				// add the polygon consisting of the two edge vertices and the
				// first extruded point
				//
				ib[ 0 ] = lastEdgeVertex2Index;  // lastedgeVertex2 index
				ib[ 4 ] = ib[ 1 ] = lastExtrude2Index;  // lastextrude2 index
				ib[ 3 ] = ib[ 2 ] = stripStartVertex;  // edgeVertex2 index
				ib[ 5 ] = stripStartVertex+1;  // extrude2 index
				ib += 6;	//skip past 2 triangles just added.
			}
			else
			{	//add end of strip.  Finishes the last 2 polygons.
				const Vector3& ev=geomMesh->GetVertex( currentEdgeEnd );
				*vb++ = *(VertexFormatXYZ *)&ev;

				//
				// add the polygon consisting of the two edge vertices and the
				// first extruded point
				//
				ib[ 0 ] = lastEdgeVertex2Index;  // lastedgeVertex2 index
				ib[ 4 ] = ib[ 1 ] = lastExtrude2Index;  // lastextrude2 index
				ib[ 3 ] = ib[ 2 ] = vertexCount;  // edgeVertex2 index
				ib[ 5 ] = vertexCount+1;  // extrude2 index
				ib += 6;	//skip past 2 triangles just added.

				// take the other edge point and extrude away from light
				extrude2 = ev - *lightPosObject;
				extrude2 *= shadowExtrudeDistance;
				extrude2 += ev;
				// add the one new vertex
				*vb++ = *(VertexFormatXYZ *)&extrude2;

				lastEdgeVertex2Index=vertexCount;
				lastExtrude2Index=vertexCount+1;
				vertexCount += 2;
			}

			if ((i+2) >= indicesPerMesh)
			{	//finished with all silhouette edges
				polygonCount += 2;
				break;	//reached end of all edges
			}

			//Start a new strip by adding first vertex and extrusion.
			const Vector3& evb=geomMesh->GetVertex( silhouetteIndices[ i+2 ] );
			// take one edge point and extrude away from the light
			extrude2 = evb - *lightPosObject;
			extrude2 *= shadowExtrudeDistance;
			extrude2 += evb;

			lastEdgeVertex2Index=vertexCount;
			lastExtrude2Index=vertexCount + 1;
			//record start of new strip info
			stripStartIndex=silhouetteIndices[ i+2 ];
			stripStartVertex=lastEdgeVertex2Index;

			*vb++ = *(VertexFormatXYZ *)&evb;
			*vb++ = *(VertexFormatXYZ *)&extrude2;
			vertexCount += 2;

			polygonCount += 2;
			continue;
		}
		else
		{	//continue existing strip by adding extra vertex and extrusion

			const Vector3& ev=geomMesh->GetVertex( currentEdgeEnd );
			*vb++ = *(VertexFormatXYZ *)&ev;
			//
			// add the polygon consisting of the two edge vertices and the
			// first extruded point
			//
			ib[ 0 ] = lastEdgeVertex2Index;  // lastedgeVertex2 index
			ib[ 4 ] = ib[ 1 ] = lastExtrude2Index;  // lastextrude2 index
			ib[ 3 ] = ib[ 2 ] = vertexCount;  // edgeVertex2 index
			ib[ 5 ] = vertexCount+1;  // extrude2 index
			ib += 6;	//skip past 2 triangles just added

			// take the other edge point and extrude away from light
			extrude2 = ev - *lightPosObject;
			extrude2 *= shadowExtrudeDistance;
			extrude2 += ev;

			// add the one new vertex
			*vb++ = *(VertexFormatXYZ *)&extrude2;

			lastEdgeVertex2Index=vertexCount;
			lastExtrude2Index=vertexCount+1;

			vertexCount += 2;
			polygonCount += 2;
		}
	}

//	DEBUG_ASSERTLOG(polygonCount == vertexCount, ("WARNING***Shadow volume mesh not optimal: %s",m_geometry->Get_Name()));
}

// allocateShadowVolume =======================================================
// Allocate a space for us to construct the shadow volume in
// ============================================================================
Bool W3DVolumetricShadow::allocateShadowVolume( Int volumeIndex, Int meshIndex )
{
	Int numVertices, numPolygons;
	Geometry *shadowVolume;

	// sanity
	if( volumeIndex < 0 || volumeIndex >= MAX_SHADOW_LIGHTS )
	{

//		DBGPRINTF(( "Illegal allocate shadow volume index '%d'\n", volumeIndex ));
		assert( 0 );
		return FALSE;

	}

	if ((shadowVolume = m_shadowVolume[ volumeIndex ][meshIndex]) == nullptr)
	{
		// poolify
		shadowVolume = NEW Geometry;		// create the new geometry
		// we now have one more valid geometry volume
		m_shadowVolumeCount[meshIndex]++;
	}

	if( shadowVolume == nullptr )
	{

//		DBGPRINTF(( "Unable to allocate '%d' shadow volume\n", volumeIndex ));
		assert( 0 );
		// we now have one more valid geometry volume
		m_shadowVolumeCount[meshIndex]--;
		return FALSE;

	}

	// assign to list
	m_shadowVolume[ volumeIndex ][meshIndex] = shadowVolume;

	//
	// polygons are determined from the edges extruded from the light.
	// since we have a list of disjoint edge pairs we will have a 4 sided
	// polygon for each edge pair, however we will be splitting the
	// 4 sided polys into 2 triangles so it just works out that num polys
	// is the number of silhouette indices
	//
	numPolygons = m_maxSilhouetteEntries[meshIndex];

	//
	// vertices are an extrusion of the edges, and since we have disjoint
	// pairs of edge indices we will have num indices * 2 actual vertex
	// points.  it may be a good future optimization to not duplicate
	// any vertices that appear twice here, but then again the cost over
	// optimizing this routine over the rendering and transformation
	// optimization may not be worth it
	//
	numVertices = m_maxSilhouetteEntries[meshIndex] * 2;

	//Only allocate space here for dynamic shadows.  Shadows for static/non-animated
	//models will be stored in vertex buffers which are allocated once exact size
	//is known.
	if (shadowVolume->GetFlags() & SHADOW_DYNAMIC)
	{
		//for dynamic shadow casters, we need to allocate the maximum amount of vertices that could ever be required.
//		if (m_shadowVolumeVB[ volumeIndex ][meshIndex])
//			TheW3DBufferManager->releaseSlot(m_shadowVolumeVB[ volumeIndex ][meshIndex]);
//		m_shadowVolumeVB[ volumeIndex ][meshIndex] = TheW3DVertexBufferManager->getSlot(W3DVertexBufferManager::VBM_FVF_XYZ, numVertices);

		// allocate memory for the vertices and polygons
		if( shadowVolume->Create( numVertices, numPolygons ) == FALSE )
		{

	//		DBGPRINTF(( "Unable to create shadow volume\n" ));
			assert( 0 );
			delete shadowVolume;
			return FALSE;

		}
	}

	return TRUE;  // success

}

// deleteShadowVolume =========================================================
// Free all resources allocated to the shadow volume(s)
// ============================================================================
void W3DVolumetricShadow::deleteShadowVolume( Int volumeIndex )
{

	// sanity
	if( volumeIndex < 0 || volumeIndex >= MAX_SHADOW_LIGHTS )
	{

//		DBGPRINTF(( "Illegal delete shadow volume index '%d'\n", volumeIndex ));
		assert( 0 );
		return;

	}

	// delete it!
	for (Int meshIndex=0; meshIndex<MAX_SHADOW_CASTER_MESHES; meshIndex++)
	{
		if( m_shadowVolume[ volumeIndex ][meshIndex] )
		{

			delete m_shadowVolume[ volumeIndex ][meshIndex];
			m_shadowVolume[ volumeIndex ][meshIndex] = nullptr;

			// we now have one less shadow volume
			m_shadowVolumeCount[meshIndex]--;

		}
	}

}

// resetShadowVolume ==========================================================
// Reset the contents of the shadow volume information.  Since we're using
// a geometry class it would be ideal if these structures had a reset
// option where their resources were released back to a pool rather than
// delete and allocate new storage space
// ============================================================================
void W3DVolumetricShadow::resetShadowVolume( Int volumeIndex, Int meshIndex )
{
	Geometry *geometry;

	// sanity
	if( volumeIndex < 0 || volumeIndex >= MAX_SHADOW_LIGHTS )
	{

//		DBGPRINTF(( "Illegal reset shadow volume index '%d'\n", volumeIndex ));
		assert( 0 );
		return;

	}

	geometry = m_shadowVolume[ volumeIndex ][meshIndex];

	//Release buffers used to hold shadow volume geometry
	if (geometry)
	{	if (m_shadowVolumeVB[volumeIndex][meshIndex])
		{	TheW3DBufferManager->releaseSlot(m_shadowVolumeVB[volumeIndex][meshIndex]);
			m_shadowVolumeVB[volumeIndex][meshIndex]=nullptr;
		}
		if (m_shadowVolumeIB[ volumeIndex ][meshIndex])
		{	TheW3DBufferManager->releaseSlot(m_shadowVolumeIB[volumeIndex][meshIndex]);
			m_shadowVolumeIB[volumeIndex][meshIndex]=nullptr;
		}
		geometry->Release();
	}

}

// allocateSilhouette =========================================================
// Allocate space for new silhouette storage, the number of vertices passed
// in is the total vertices in the model, a silhouette must be able to
// accommodate that as a series of disjoint edge pairs, otherwise known
// as numVertices * 2
// ============================================================================
Bool W3DVolumetricShadow::allocateSilhouette(Int meshIndex, Int numVertices )
{
	Int numEntries = numVertices * 5;	///@todo: HACK, HACK... Should be 2!

	// sanity
	assert( m_silhouetteIndex[meshIndex] == nullptr &&
					m_numSilhouetteIndices[meshIndex] == 0 &&
					numEntries > 0 );

	// allocate memory
	m_silhouetteIndex[meshIndex] = NEW short[ numEntries ];
	if( m_silhouetteIndex[meshIndex] == nullptr )
	{

//		DBGPRINTF(( "Unable to allocate silhouette storage '%d'\n", numEntries ));
		assert( 0 );
		return FALSE;

	}

	// set our list to empty just to be clean
	m_numSilhouetteIndices[meshIndex] = 0;

	// save the size of our silhouette list
	m_maxSilhouetteEntries[meshIndex] = numEntries;

	return TRUE;  // success

}

// deleteSilhouette ===========================================================
// Delete all silhouette data and memory allocated
// ============================================================================
void W3DVolumetricShadow::deleteSilhouette( Int meshIndex )
{

	delete [] m_silhouetteIndex[meshIndex];
	m_silhouetteIndex[meshIndex] = nullptr;
	m_numSilhouetteIndices[meshIndex] = 0;

}

// resetSilhouette ============================================================
// Resets the silhouette to empty, it does NOT free any of the memory
// allocated for silhouette data
// ============================================================================
void W3DVolumetricShadow::resetSilhouette( Int meshIndex )
{

	m_numSilhouetteIndices[meshIndex] = 0;

}

// renderStencilShadows =======================================================
// The stencil buffer now has our shadow information in it, take that
// info and draw a big transparent rectangle over the screen for the final
// shadow pass wherever there is data in the stencil buffer
// ============================================================================
void W3DVolumetricShadowManager::renderStencilShadows()
{
	LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();

	if (!m_pDev)
		return;	//need device to render anything.

	struct _TRANSLITVERTEX {
	    D3DXVECTOR4 p;
		DWORD color;   // diffuse color
	} v[4];

	Int xpos, ypos, width, height;

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

    v[0].p = D3DXVECTOR4( xpos+width, ypos+height, 0.0f, 1.0f );
    v[1].p = D3DXVECTOR4( xpos+width, 0, 0.0f, 1.0f );
    v[2].p = D3DXVECTOR4(  xpos, ypos+height, 0.0f, 1.0f );
    v[3].p = D3DXVECTOR4(  xpos,  0, 0.0f, 1.0f );
    v[0].color = TheW3DShadowManager->getShadowColor();
    v[1].color = TheW3DShadowManager->getShadowColor();
    v[2].color = TheW3DShadowManager->getShadowColor();
    v[3].color = TheW3DShadowManager->getShadowColor();

	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
	m_pDev->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

	// Use alpha blending to draw the transparent shadow
    m_pDev->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
//  m_pDev->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
//  m_pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		m_pDev->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_DESTCOLOR);
		m_pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );


	// Set stencil states
    m_pDev->SetRenderState( D3DRS_ZENABLE,          TRUE );
		m_pDev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);

	// Only write where stencil val >= 1 (count indicates # of shadows that
	// overlap that pixel)
    m_pDev->SetRenderState( D3DRS_STENCILENABLE, TRUE );
    m_pDev->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_LESSEQUAL );	//reference value is less or equal to stencil
    m_pDev->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
	//Upper bits of stencil could be used for storing occluded models which are player colored.  So we mask out those
	//pixels and only use the lower bits for shadow calculations.
	m_pDev->SetRenderState( D3DRS_STENCILMASK,     ~TheW3DShadowManager->getStencilShadowMask());
    m_pDev->SetRenderState( D3DRS_STENCILREF,      0x1 );


	m_pDev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);

	if (DX8Wrapper::_Is_Triangle_Draw_Enabled())
		m_pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANSLITVERTEX));

	m_pDev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	m_pDev->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
	// turn off the stencil buffer
	m_pDev->SetRenderState( D3DRS_STENCILENABLE, FALSE );

}

void W3DVolumetricShadowManager::renderShadows( Bool forceStencilFill )
{
	W3DVolumetricShadow *shadow;
	Int numRenderedShadows = 0;

 	AABoxClass bbox;
	SphereClass bsphere;

 	//Get a bounding box around our visible universe.  Bounded by terrain and the sky
 	//so much tighter fitting volume than what's actually visible.  This will cull
 	//particles falling under the ground.

 	TheTerrainRenderObject->getMaximumVisibleBox(*shadowCameraFrustum, &bbox, TRUE);

 	bcX = bbox.Center.X;
 	bcY = bbox.Center.Y;
 	bcZ = bbox.Center.Z;
 	beX = bbox.Extent.X;
 	beY = bbox.Extent.Y;
 	beZ = bbox.Extent.Z;

	if (m_shadowList && TheGlobalData->m_useShadowVolumes)
	{

		LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();

		if (!m_pDev)
			return;	//need device to render anything.

 		//According to Nvidia there's a D3D bug that happens if you don't start with a
 		//new dynamic VB each frame - so we force a DISCARD by overflowing the counter.
 		nShadowIndicesInBuf = 0xffff;
 		nShadowVertsInBuf = 0xffff;

		//Set W3D to some known state
		VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
		DX8Wrapper::Set_Material(vmat);
		REF_PTR_RELEASE(vmat);

		DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader);
		DX8Wrapper::Set_Texture(0,nullptr);	//turn off textures
		DX8Wrapper::Set_Texture(1,nullptr);	//turn off textures
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

		// turn off z writing
		m_pDev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	  m_pDev->SetRenderState( D3DRS_ZENABLE,          TRUE );
		m_pDev->SetRenderState(D3DRS_ZWRITEENABLE , FALSE);
		m_pDev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		m_pDev->SetRenderState(D3DRS_FOGENABLE, FALSE);


		// setup the TMU to default
		m_pDev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
		m_pDev->SetRenderState(D3DRS_LIGHTING, FALSE);
		m_pDev->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		m_pDev->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		m_pDev->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2);
		m_pDev->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
		m_pDev->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );

		m_pDev->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
		m_pDev->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
		m_pDev->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
		m_pDev->SetTexture(0,nullptr);
		m_pDev->SetTexture(1,nullptr);

		DWORD oldColorWriteEnable=0x12345678;

	#ifdef SV_DEBUG
		m_pDev->SetRenderState(D3DRS_ALPHABLENDENABLE , TRUE);
		m_pDev->SetRenderState( D3DRS_STENCILENABLE, FALSE );
		m_pDev->SetRenderState( D3DRS_SRCBLEND, /*D3DBLEND_DESTCOLOR*/D3DBLEND_ONE );
		m_pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
		m_pDev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	#else
		//disable writes to color buffer
		if (DX8Wrapper::Get_Current_Caps()->Get_DX8_Caps().PrimitiveMiscCaps & D3DPMISCCAPS_COLORWRITEENABLE)
		{	DX8Wrapper::_Get_D3D_Device8()->GetRenderState(D3DRS_COLORWRITEENABLE, &oldColorWriteEnable);
			DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,0);
		}
		else
		{	//device does not support disabling writes to color buffer so fake it through alpha blending
			m_pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ZERO );
			m_pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
			m_pDev->SetRenderState(D3DRS_ALPHABLENDENABLE , TRUE);
		}
		m_pDev->SetRenderState( D3DRS_STENCILENABLE, TRUE );
	#endif
		//Any pixels with stencil already set to 128 contains a potential occluder.  If this pixels also has any of the player
		//color stencil bits also set, it means that it's an occluded player color and we need to NOT render shadows here.  We
		//do this determination by comparing the value in the combined bits against a value containing only a potential occluder.
		//If the value of just the potential occluder bit is >= than the combined bits, then we know none of the player color
		//bits were set and it's okay to render shadow.
		if (TheW3DShadowManager->getStencilShadowMask() == 0x80808080)
			m_pDev->SetRenderState( D3DRS_STENCILFUNC,     D3DCMP_NOTEQUAL );	//in this mode, MSB indicates occluded player pixels.
		else
			m_pDev->SetRenderState( D3DRS_STENCILFUNC,     D3DCMP_GREATEREQUAL );	//in this mode, multiple bits indicate occluded player pixels.
		m_pDev->SetRenderState( D3DRS_STENCILREF,      0x80808080 );			//isolate MSB, it's used to indicate pixels containing potential occluders.
		m_pDev->SetRenderState( D3DRS_STENCILMASK,     TheW3DShadowManager->getStencilShadowMask());	//isolate upper bits containing PotentialOccluderBit|PlayerColorBits
		m_pDev->SetRenderState( D3DRS_STENCILWRITEMASK,0xffffffff );
		m_pDev->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
		m_pDev->SetRenderState( D3DRS_STENCILFAIL,  D3DSTENCILOP_KEEP );
		m_pDev->SetRenderState( D3DRS_STENCILPASS,  D3DSTENCILOP_INCR );

		m_pDev->SetVertexShader(SHADOW_DYNAMIC_VOLUME_FVF);

		m_pDev->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
//		m_pDev->SetRenderState(D3DRS_ZBIAS,1);	///@todo: See if this helps or makes things worse.
		//m_pDev->SetRenderState(D3DRS_FILLMODE,D3DFILL_WIREFRAME);


		lastActiveVertexBuffer=nullptr;	//reset

		m_dynamicShadowVolumesToRender=nullptr;	//clear list of pending dynamic shadows
		W3DVolumetricShadowRenderTask *shadowDynamicTasksStart,*shadowDynamicTask;

		// step through each of our shadows and render
		for( shadow = m_shadowList; shadow; shadow = shadow->m_next )
		{
			if (shadow->m_isEnabled && !shadow->m_isInvisibleEnabled)
			{
				//Record last added task
				shadowDynamicTasksStart=m_dynamicShadowVolumesToRender;
				shadow->Update();
				shadowDynamicTask=m_dynamicShadowVolumesToRender;
				while (shadowDynamicTask != shadowDynamicTasksStart)
				{	//update() added a dynamic shadow
					//dynamic shadow columns don't need to wait in queue since they
					//all use the same vertex buffer.  Flush them ASAP.
					shadow->RenderVolume(shadowDynamicTask->m_meshIndex,shadowDynamicTask->m_lightIndex);
					//move to next dynamic task
					shadowDynamicTask=(W3DVolumetricShadowRenderTask *)shadowDynamicTask->m_nextTask;
					numRenderedShadows++;
				}
			}
		}

		// Set vertex format to that used by static shadow volumes
		m_pDev->SetVertexShader(W3DBufferManager::getDX8Format(W3DBufferManager::VBM_FVF_XYZ));

		//Empty queue of static shadow volumes to render.
		W3DBufferManager::W3DVertexBuffer *nextVb;
		W3DVolumetricShadowRenderTask *nextTask;
		for (nextVb=TheW3DBufferManager->getNextVertexBuffer(nullptr,W3DBufferManager::VBM_FVF_XYZ);nextVb != nullptr; nextVb=TheW3DBufferManager->getNextVertexBuffer(nextVb,W3DBufferManager::VBM_FVF_XYZ))
		{
			nextTask=(W3DVolumetricShadowRenderTask *)nextVb->m_renderTaskList;
			while (nextTask)
			{
				nextTask->m_parentShadow->RenderVolume(nextTask->m_meshIndex,nextTask->m_lightIndex);
				nextTask=(W3DVolumetricShadowRenderTask *)nextTask->m_nextTask;
				numRenderedShadows++;
			}
		}

		// change the stencil op to decrement
		m_pDev->SetRenderState( D3DRS_STENCILPASS,  D3DSTENCILOP_DECRSAT);

		//
		// invert normals of shadow volumes so we can decrement in the
		// stencil buffer and render
		//

		m_pDev->SetRenderState(D3DRS_CULLMODE,D3DCULL_CCW);

		for (nextVb=TheW3DBufferManager->getNextVertexBuffer(nullptr,W3DBufferManager::VBM_FVF_XYZ);nextVb != nullptr; nextVb=TheW3DBufferManager->getNextVertexBuffer(nextVb,W3DBufferManager::VBM_FVF_XYZ))
		{
			nextTask=(W3DVolumetricShadowRenderTask *)nextVb->m_renderTaskList;
			while (nextTask)
			{
				nextTask->m_parentShadow->RenderVolume(nextTask->m_meshIndex,nextTask->m_lightIndex);
				nextTask=(W3DVolumetricShadowRenderTask *)nextTask->m_nextTask;
			}
		}

		m_pDev->SetVertexShader(SHADOW_DYNAMIC_VOLUME_FVF);
		//flush any dynamic shadow volumes
		shadowDynamicTask=m_dynamicShadowVolumesToRender;
		while (shadowDynamicTask)
		{	//dynamic shadow columns don't need to wait in queue since they
			//all use the same vertex buffer.  Flush them ASAP.
			shadowDynamicTask->m_parentShadow->RenderVolume(shadowDynamicTask->m_meshIndex,shadowDynamicTask->m_lightIndex);
			shadowDynamicTask=(W3DVolumetricShadowRenderTask *)shadowDynamicTask->m_nextTask;
		}

		//Reset all render tasks for next frame.
		for (nextVb=TheW3DBufferManager->getNextVertexBuffer(nullptr,W3DBufferManager::VBM_FVF_XYZ);nextVb != nullptr; nextVb=TheW3DBufferManager->getNextVertexBuffer(nextVb,W3DBufferManager::VBM_FVF_XYZ))
		{
			nextVb->m_renderTaskList=nullptr;
		}

		m_pDev->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
//		m_pDev->SetRenderState(D3DRS_ZBIAS,0);	///@todo: See if this helps or makes things worse.
		//m_pDev->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);


		if (oldColorWriteEnable != 0x12345678)
			DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,oldColorWriteEnable);

		//
		// render the big transparent square of shadows in the stencil buffer
		// to the screen
		//
///@todo: Put this check back in after water is fixed so it doesn't require shadow rendering to fix alpha.
//		if (numRenderedShadows)
			renderStencilShadows();

		m_pDev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
		m_pDev->SetRenderState(D3DRS_ALPHABLENDENABLE , FALSE);
		m_pDev->SetRenderState(D3DRS_LIGHTING, FALSE);

		DX8Wrapper::Invalidate_Cached_Render_States();
	}
	else
	if (forceStencilFill)
	{	//no shadows to render, but still need to fill stencil buffer
		//for other effects.

		//Set W3D to some known state
		VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
		DX8Wrapper::Set_Material(vmat);
		REF_PTR_RELEASE(vmat);
		DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader);
		DX8Wrapper::Set_Texture(0,nullptr);
		DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

		renderStencilShadows();

		DX8Wrapper::Invalidate_Cached_Render_States();
	}

}

/** This class will manage shadow geometry for each render object.  Shadow geometry may
be the same as render geometry but doesn't need to be.  This allows lower LOD versions of
the geometry to be used in shadow calculations.  Shadow geometry also keeps extended vertex
connectivity information that's not used during rendering.
*/
class W3DShadowGeometryManager
{
public:
	W3DShadowGeometryManager();
	~W3DShadowGeometryManager();

	int			 		Load_Geom(RenderObjClass *robj, const char *name);
	W3DShadowGeometry *		Get_Geom(const char * name);
	W3DShadowGeometry *		Peek_Geom(const char * name);
	Bool					Add_Geom(W3DShadowGeometry *new_anim);
	void			 		Free_All_Geoms();

	void					Register_Missing( const char * name );
	Bool					Is_Missing( const char * name );
	void					Reset_Missing();

private:

	HashTableClass	*	GeomPtrTable;
	HashTableClass	*	MissingGeomTable;

	friend	class		W3DShadowGeometryManagerIterator;
};

/*
** An Iterator to get to all loaded W3DShadowGeometries in a W3DShadowGeometryManager
*/
class W3DShadowGeometryManagerIterator : public HashTableIteratorClass {
public:
	W3DShadowGeometryManagerIterator( W3DShadowGeometryManager & manager ) : HashTableIteratorClass( *manager.GeomPtrTable ) {}
	W3DShadowGeometry * Get_Current_Geom();
};

/** Used to cause a rebuild of all shadow volumes*/
void W3DVolumetricShadowManager::invalidateCachedLightPositions()
{

	if (!m_shadowList)
		return;	//there are no shadows to render.

	W3DVolumetricShadow *shadow;
	Vector3 vec(0,0,0);

	// step through each of our shadows and update previous light position.
	for( shadow = m_shadowList; shadow; shadow = shadow->m_next )
	{
		for(Int i = 0; i < MAX_SHADOW_LIGHTS; i++ )
		{
			for (Int meshIndex=0; meshIndex<MAX_SHADOW_CASTER_MESHES; meshIndex++)
			{
				shadow->setLightPosHistory(i,meshIndex,vec);
			}
		}
	}
}

// W3DVolumetricShadowManager =============================================================
// ============================================================================
W3DVolumetricShadowManager::W3DVolumetricShadowManager()
{

	m_shadowList = nullptr;

	m_W3DShadowGeometryManager = NEW W3DShadowGeometryManager;

	TheW3DBufferManager = NEW W3DBufferManager;

}

// ~W3DVolumetricShadowManager ============================================================
// ============================================================================
W3DVolumetricShadowManager::~W3DVolumetricShadowManager()
{
	ReleaseResources();
	delete m_W3DShadowGeometryManager;
	m_W3DShadowGeometryManager = nullptr;
	delete TheW3DBufferManager;
	TheW3DBufferManager=nullptr;

	//all shadows should be freed up at this point but check anyway
	assert(m_shadowList==nullptr);

}

/** Releases all W3D/D3D assets before a reset.. */
void W3DVolumetricShadowManager::ReleaseResources()
{
	if (shadowIndexBufferD3D)
		shadowIndexBufferD3D->Release();
	if (shadowVertexBufferD3D)
		shadowVertexBufferD3D->Release();
	shadowIndexBufferD3D=nullptr;
	shadowVertexBufferD3D=nullptr;
	if (TheW3DBufferManager)
	{	TheW3DBufferManager->ReleaseResources();
		invalidateCachedLightPositions();	//vertex buffers need to be refilled.
	}
}

/** (Re)allocates all W3D/D3D assets after a reset.. */
Bool W3DVolumetricShadowManager::ReAcquireResources()
{
	ReleaseResources();

	LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();

	DEBUG_ASSERTCRASH(m_pDev, ("Trying to ReAcquireResources on W3DVolumetricShadowManager without device"));

	if (FAILED(m_pDev->CreateIndexBuffer
	(
		SHADOW_INDEX_SIZE*sizeof(WORD),
		D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC,
		D3DFMT_INDEX16,
		D3DPOOL_DEFAULT,
		&shadowIndexBufferD3D
	)))
		return FALSE;

	if (shadowVertexBufferD3D == nullptr)
	{	// Create vertex buffer

		if (FAILED(m_pDev->CreateVertexBuffer
		(
			SHADOW_VERTEX_SIZE*sizeof(SHADOW_DYNAMIC_VOLUME_VERTEX),
			D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC,
			0,
			D3DPOOL_DEFAULT,
			&shadowVertexBufferD3D
		)))
			return FALSE;
	}

	if (TheW3DBufferManager)
		if (!TheW3DBufferManager->ReAcquireResources())
			return FALSE;

	return TRUE;
}

// Init =======================================================================
// User called initialization
// ============================================================================
Bool W3DVolumetricShadowManager::init()
{
	return TRUE;
}

// Reset ======================================================================
// Reset our list of shadows to empty
// ============================================================================
void W3DVolumetricShadowManager::reset()
{

	assert (m_shadowList == nullptr);
	m_W3DShadowGeometryManager->Free_All_Geoms();
	TheW3DBufferManager->freeAllBuffers();

}

// addShadow ==================================================================
// Add the shadows for this hierarchy to the shadow management for
// rendering.
// ============================================================================
W3DVolumetricShadow* W3DVolumetricShadowManager::addShadow(RenderObjClass *robj, Shadow::ShadowTypeInfo *shadowInfo, Drawable *draw)
{
	if (!DX8Wrapper::Has_Stencil() || !robj || !TheGlobalData->m_useShadowVolumes)
		return nullptr;	//right now we require a stencil buffer

	W3DShadowGeometry *sg=nullptr;
	if (!robj)
		return nullptr;	//must have a render object in order to read shadow geometry

	const char *name=robj->Get_Name();

	if (!name)
		return nullptr;

	sg=m_W3DShadowGeometryManager->Get_Geom(name);

	if (sg==nullptr)
	{	//did not find a cached copy of the shadow geometry, create a new one
		m_W3DShadowGeometryManager->Load_Geom(robj,name);
		//try loading again
		sg=m_W3DShadowGeometryManager->Get_Geom(name);
		if (sg==nullptr)
			return nullptr;	//could not create the shadow geometry
	}

	W3DVolumetricShadow *shadow = NEW W3DVolumetricShadow;	// poolify

	// sanity
	if( shadow == nullptr )
		return nullptr;

	shadow->setRenderObject(robj);
	shadow->SetGeometry(sg);
 	SphereClass sphere;
 	robj->Get_Obj_Space_Bounding_Sphere(sphere);
 	shadow->setRenderObjExtent(sphere.Radius*MAX_SHADOW_LENGTH_SCALE_FACTOR);

	Real sunElevationAngleTan = 0;
	if (shadowInfo->m_sizeX)
	{	//need to adjust sun elevation for this model in order to limit shadow length
		sunElevationAngleTan=tan(shadowInfo->m_sizeX/180.0f*PI);
	}
	shadow->setShadowLengthScale(sunElevationAngleTan);

	if (!draw || !draw->isKindOf(KINDOF_IMMOBILE))
		shadow->setOptimalExtrusionPadding(SHADOW_EXTRUSION_BUFFER);

	// add to our shadow list through the shadow next links
	shadow->m_next = m_shadowList;
	m_shadowList = shadow;
	return shadow;
}

/** removeShadow ===========================================================
 Removes the shadows for this hierarchy from the shadow manger.  No further
 shadows from this caster will be rendered.
 ===========================================================================
*/
void W3DVolumetricShadowManager::removeShadow(W3DVolumetricShadow *shadow)
{
	W3DVolumetricShadow *prev_shadow=nullptr;
	W3DVolumetricShadow *next_shadow=nullptr;

	//search for this shadow
	for( next_shadow = m_shadowList; next_shadow; prev_shadow=next_shadow, next_shadow = next_shadow->m_next )
	{
		if (next_shadow == shadow)
		{
			if (prev_shadow)
				prev_shadow->m_next=shadow->m_next;
			else
				m_shadowList=shadow->m_next;

			delete shadow;
			break;
		}
	}
}

/** removeAllShadows ===========================================================
 Removes all shadows from the shadow manger.  No further
 shadows will be rendered.
 ===========================================================================
*/
void W3DVolumetricShadowManager::removeAllShadows()
{
	W3DVolumetricShadow *cur_shadow=nullptr;
	W3DVolumetricShadow *next_shadow=m_shadowList;
	m_shadowList = nullptr;

	//search for this shadow
	for( cur_shadow = next_shadow; cur_shadow; cur_shadow = next_shadow )
	{
		next_shadow = cur_shadow->m_next;
		cur_shadow->m_next = nullptr;
		delete cur_shadow;
	}
}

W3DShadowGeometryManager::W3DShadowGeometryManager()
{
	// Create the hash tables
	GeomPtrTable = NEW HashTableClass( 2048 );
	MissingGeomTable = NEW HashTableClass( 2048 );
}

W3DShadowGeometryManager::~W3DShadowGeometryManager()
{
	Free_All_Geoms();

	delete GeomPtrTable;
	GeomPtrTable = nullptr;

	delete MissingGeomTable;
	MissingGeomTable = nullptr;
}

/** Release all loaded animations */
void W3DShadowGeometryManager::Free_All_Geoms()
{
	// Make an iterator, and release all ptrs
	W3DShadowGeometryManagerIterator it( *this );
	for( it.First(); !it.Is_Done(); it.Next() ) {
		W3DShadowGeometry *geom = it.Get_Current_Geom();
		geom->Release_Ref();
	}

	// Then clear the table
	GeomPtrTable->Reset();
}

/** Find animation in cache */
W3DShadowGeometry * W3DShadowGeometryManager::Peek_Geom(const char * name)
{
	return (W3DShadowGeometry*)GeomPtrTable->Find( name );
}

/** Get animation from cache and increment its reference count */
W3DShadowGeometry * W3DShadowGeometryManager::Get_Geom(const char * name)
{
	W3DShadowGeometry * geom = Peek_Geom( name );
	if ( geom != nullptr ) {
		geom->Add_Ref();
	}
	return geom;
}

/** Add animation to cache */
Bool W3DShadowGeometryManager::Add_Geom(W3DShadowGeometry *new_geom)
{
	WWASSERT (new_geom != nullptr);

	// Increment the refcount on the new animation and add it to our table.
	new_geom->Add_Ref ();
	GeomPtrTable->Add( new_geom );

	return true;
}

/*
** An entry for a table of anims not found, so we can quickly determine their loss
*/
class MissingGeomClass : public HashableClass {

public:
	MissingGeomClass( const char * name ) : Name( name ) {}
	virtual	~MissingGeomClass() override {}

	virtual	const char * Get_Key() override { return Name;	}

private:
	StringClass	Name;

};

/*
** Missing Geoms
**
** The idea here, allow the system to register which anims are determined to be missing
** so that if they are asked for again, we can quickly return nullptr, without searching the
** disk again.
*/
void	W3DShadowGeometryManager::Register_Missing( const char * name )
{
	MissingGeomTable->Add( NEW MissingGeomClass( name ) );
}

Bool	W3DShadowGeometryManager::Is_Missing( const char * name )
{
	return ( MissingGeomTable->Find( name ) != nullptr );
}

/** Create shadow geometry from a reference W3D RenderObject*/
int W3DShadowGeometryManager::Load_Geom(RenderObjClass *robj, const char *name)
{
	Bool res=FALSE;

	W3DShadowGeometry * newgeom = NEW W3DShadowGeometry;

	if (newgeom == nullptr) {
		goto Error;
	}

	SET_REF_OWNER( newgeom );

	newgeom->Set_Name(name);

	switch (robj->Class_ID())
	{
		case RenderObjClass::CLASSID_HLOD:
			res=newgeom->initFromHLOD(robj);
			break;
		case RenderObjClass::CLASSID_MESH:
			res=newgeom->initFromMesh(robj);
			break;
		default:
			break;	//unknown render object type
	};

	if (res != TRUE)
	{	// load failed!
		newgeom->Release_Ref();
		//DEBUG_LOG(("****Shadow Volume Creation Failed on %s",name));
		goto Error;
	} else if (Peek_Geom(newgeom->Get_Name()) != nullptr)
	{	// duplicate exists!
		newgeom->Release_Ref();	// Release the one we just loaded
		goto Error;
	} else
	{	Add_Geom( newgeom );
		newgeom->Release_Ref();
	}

	return 0;

Error:
	return 1;
}

/*
** Iterator converter from HashableClass to W3DShadowGeometry
*/
W3DShadowGeometry * W3DShadowGeometryManagerIterator::Get_Current_Geom()
{
	return (W3DShadowGeometry *)Get_Current();
}
