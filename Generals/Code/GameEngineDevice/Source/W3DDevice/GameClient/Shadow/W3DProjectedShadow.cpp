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

;////////////////////////////////////////////////////////////////////////////////
;//																																						 //
;//  (c) 2001-2003 Electronic Arts Inc.																				 //
;//																																						 //
;////////////////////////////////////////////////////////////////////////////////

// FILE: W3DTextureShadow.cpp ///////////////////////////////////////////////////////////
//
// Texture based shadow representation.
//
// Author: Mark Wilczynski, February 2002
//
//
///////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "always.h"
#include "GameClient/View.h"
#include "WW3D2/camera.h"
#include "WW3D2/light.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/hlod.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/texproject.h"
#include "WW3D2/dx8renderer.h"
#include "Lib/BaseType.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "d3dx8math.h"
#include "Common/GlobalData.h"
#include "W3DDevice/GameClient/W3DProjectedShadow.h"
#include "WW3D2/statistics.h"
#include "Common/Debug.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameClient/Drawable.h"
#include "W3DDevice/GameClient/Module/W3DModelDraw.h"
#include "W3DDevice/GameClient/W3DShadow.h"


/** @todo: We're going to have a pool of a couple rendertargets to use
in rare cases when dynamic shadows need to be generated.  Maybe we can
even get away with a single one that gets used immediately to render, then
recycled.  For most of the objects, we need to have a static texture that
is reused for all instances on the level.

Need to add support for loading textures from disk instead of generating them in
code.  Could allow for a single non-distinct blob to be used for everything.

Instead of projecting onto arbitrary geometry, could allow for terrain only.
Maybe project onto a deformed terrain patch that molds to trays/bibs.
*/

#define DEFAULT_RENDER_TARGET_WIDTH			512
#define DEFAULT_RENDER_TARGET_HEIGHT		512

W3DProjectedShadowManager *TheW3DProjectedShadowManager=nullptr;	//global singleton
ProjectedShadowManager	*TheProjectedShadowManager;				//global singleton with simpler interface.
extern const FrustumClass *shadowCameraFrustum;	//defined in W3DShadow.
///@todo: Externs from volumetric shadow renderer - these need to be moved into W3DBufferManager
extern LPDIRECT3DVERTEXBUFFER8 shadowVertexBufferD3D;		///<D3D vertex buffer
extern LPDIRECT3DINDEXBUFFER8	shadowIndexBufferD3D;	///<D3D index buffer
extern int nShadowVertsInBuf;	//model vetices in vertex buffer
extern int nShadowStartBatchVertex;
extern int nShadowIndicesInBuf;	//model vetices in vertex buffer
extern int nShadowStartBatchIndex;
extern int SHADOW_VERTEX_SIZE;
extern int SHADOW_INDEX_SIZE;

//Global streaming vertex buffer with x,y,z,u,v type.
struct SHADOW_DECAL_VERTEX	//vertex structure passed to D3D
{
		float x,y,z;
		DWORD diffuse;
		float u,v;
};

#define SHADOW_DECAL_FVF	D3DFVF_XYZ|D3DFVF_TEX1|D3DFVF_DIFFUSE

LPDIRECT3DVERTEXBUFFER8 shadowDecalVertexBufferD3D=nullptr;		///<D3D vertex buffer
LPDIRECT3DINDEXBUFFER8	shadowDecalIndexBufferD3D=nullptr;	///<D3D index buffer
int nShadowDecalVertsInBuf=0;	//model vetices in vertex buffer
int nShadowDecalStartBatchVertex=0;
int nShadowDecalIndicesInBuf=0;	//model vetices in vertex buffer
int nShadowDecalStartBatchIndex=0;
int	nShadowDecalPolysInBatch=0;
int	nShadowDecalVertsInBatch=0;
int SHADOW_DECAL_VERTEX_SIZE=32768;
int SHADOW_DECAL_INDEX_SIZE=65536;


class W3DShadowTexture;	//forward reference
class W3DShadowTextureManager;	//forward reference


/** This class will manage shadow textures for each render object.  Shadow textures may
be based on render geometry but don't need to be.  This allows lower detail 'blob' textures
to be substituted to improve performance.*/
class W3DShadowTextureManager
{
public:
	W3DShadowTextureManager();
	~W3DShadowTextureManager();

	int			 		createTexture(RenderObjClass *robj, const char *name);
	W3DShadowTexture *		getTexture(const char * name);
	W3DShadowTexture *		peekTexture(const char * name);
	Bool					addTexture(W3DShadowTexture *new_texture);
	void			 		freeAllTextures();
	void					invalidateCachedLightPositions();

	void					registerMissing( const char * name );
	Bool					isMissing( const char * name );
	void					resetMissing();

private:

	HashTableClass	*	texturePtrTable;
	HashTableClass	*	missingTextureTable;

	friend	class		W3DShadowTextureManagerIterator;
};

class W3DShadowTexture : public RefCountClass, public	HashableClass
{

	public:

		W3DShadowTexture()
		{	m_lastLightPosition.Set(0,0,0); m_lastObjectOrientation.Make_Identity();
			m_shadowUV[0].Set(1.0f,0.0f,0.0f);	//u runs along world x axis
			m_shadowUV[1].Set(0.0f,-1.0f,0.0f);	//v runs along world -y axis
		}
		virtual ~W3DShadowTexture() override { REF_PTR_RELEASE(m_texture);}

		virtual	const char * Get_Key() override { return m_namebuf;	}

		Int init (RenderObjClass *robj);

		const char *		Get_Name() const	{ return m_namebuf;}
		void				Set_Name(const char *name)
		{
			strlcpy(m_namebuf,name,sizeof(m_namebuf));
		}
		TextureClass	*getTexture()	{ return m_texture;}
		void					 setTexture(TextureClass *texture)	{m_texture = texture;}
		void					 setLightPosHistory(Vector3 &pos) {m_lastLightPosition=pos;}	///<updates the last position of light
		Vector3&			 getLightPosHistory() {return m_lastLightPosition;}
		void					 setObjectOrientationHistory(Matrix3x3 &mat) {m_lastObjectOrientation=mat;}	///<updates the last position of light
		Matrix3x3&			 getObjectOrientationHistory() {return m_lastObjectOrientation;}
		SphereClass&	 getBoundingSphere()	{return m_areaEffectSphere;}
		AABoxClass&		 getBoundingBox()		{return m_areaEffectBox;}
		void	 setBoundingSphere(SphereClass &sphere)	{m_areaEffectSphere=sphere;}
		void	 setBoundingBox(AABoxClass &box)		{m_areaEffectBox=box;}
		void	 updateBounds(Vector3 &lightPos, RenderObjClass *robj);	///<update extent of shadow
		void	 setDecalUVAxis(Vector3 &u, Vector3 &v)	{ m_shadowUV[0]=u; m_shadowUV[1]=v;}
		void	 getDecalUVAxis(Vector3 *u, Vector3 *v)	{ *u=m_shadowUV[0]; *v=m_shadowUV[1];}

	private:

		char m_namebuf[2*W3D_NAME_LEN];	///<name of model hierarchy

		TextureClass *m_texture; ///<texture holding the shadow for this renderobject
		Vector3		m_lastLightPosition;		///<position of light source at time of last texture update.
		Matrix3x3	m_lastObjectOrientation;	///<orientation of shadow casting object when texture was generated.
		AABoxClass	m_areaEffectBox;			///<boundary defining object-space volume affected by shadow.
		SphereClass	m_areaEffectSphere;			///<boundary defining object-space volume affected by shadow.
		Vector3		m_shadowUV[2];		///world-space vectors defining the u and v texture coordinate axis.
};

/*
** An Iterator to get to all loaded W3DShadowGeometries in a W3DShadowGeometryManager
*/
class W3DShadowTextureManagerIterator : public HashTableIteratorClass {
public:
	W3DShadowTextureManagerIterator( W3DShadowTextureManager & manager ) : HashTableIteratorClass( *manager.texturePtrTable ) {}
	W3DShadowTexture * getCurrentTexture() { 	return (W3DShadowTexture *)Get_Current();}
};


/******************** Start of W3DProjectedShadowManager implementation ***********************/
W3DProjectedShadowManager::W3DProjectedShadowManager()
{
	m_shadowList = nullptr;
	m_decalList = nullptr;
	m_numDecalShadows = 0;
	m_numProjectionShadows  = 0;
	m_W3DShadowTextureManager = nullptr;
	m_shadowCamera = nullptr;
	m_shadowContext= nullptr;
	m_drawEdgeX = 0;
	m_drawEdgeY = 0;
	m_drawStartX = 0;
	m_drawStartY = 0;
}

W3DProjectedShadowManager::~W3DProjectedShadowManager()
{

	ReleaseResources();
	m_dynamicRenderTarget = nullptr;
	m_renderTargetHasAlpha = FALSE;
	delete m_shadowContext;
	REF_PTR_RELEASE(m_shadowCamera);
	delete m_W3DShadowTextureManager;
	m_W3DShadowTextureManager = nullptr;

	//all shadows should be freed up at this point but check anyway
	DEBUG_ASSERTCRASH(m_shadowList == nullptr, ("Destroy of non-empty projected shadow list"));
	DEBUG_ASSERTCRASH(m_decalList == nullptr, ("Destroy of non-empty projected decal list"));
}

void W3DProjectedShadowManager::reset()
{

	DEBUG_ASSERTCRASH(m_shadowList == nullptr, ("Reset of non-empty projected shadow list"));
	DEBUG_ASSERTCRASH(m_decalList == nullptr, ("Reset of non-empty projected decal list"));

	m_W3DShadowTextureManager->freeAllTextures();

}

Bool W3DProjectedShadowManager::init()
{
	m_W3DShadowTextureManager = NEW W3DShadowTextureManager;
	m_shadowCamera = NEW_REF( CameraClass, () );
	m_shadowContext= NEW SpecialRenderInfoClass(*m_shadowCamera,SpecialRenderInfoClass::RENDER_SHADOW);
	m_shadowContext->light_environment = &m_shadowLightEnv;

	return TRUE;
}


Bool W3DProjectedShadowManager::ReAcquireResources()
{
	//grab assets which don't survive a device reset and need
	//to be present for duration of game.

	///@todo: We should allocate our render target pool here.

	DEBUG_ASSERTCRASH(m_dynamicRenderTarget == nullptr, ("Acquire of existing shadow render target"));

	m_renderTargetHasAlpha=TRUE;
	if ((m_dynamicRenderTarget=DX8Wrapper::Create_Render_Target (DEFAULT_RENDER_TARGET_WIDTH, DEFAULT_RENDER_TARGET_HEIGHT, WW3D_FORMAT_A8R8G8B8)) == nullptr)
	{
			m_renderTargetHasAlpha=FALSE;

			//failed to get a render target with alpha.
			//try again without.
			m_dynamicRenderTarget=DX8Wrapper::Create_Render_Target (DEFAULT_RENDER_TARGET_WIDTH, DEFAULT_RENDER_TARGET_HEIGHT);
	}

	LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();

	DEBUG_ASSERTCRASH(m_pDev, ("Trying to ReAcquireResources on W3DProjectedShadowManager without device"));
	DEBUG_ASSERTCRASH(shadowDecalIndexBufferD3D == nullptr && shadowDecalIndexBufferD3D == nullptr, ("ReAcquireResources not released in W3DProjectedShadowManager"));

	if (FAILED(m_pDev->CreateIndexBuffer
	(
		SHADOW_DECAL_INDEX_SIZE*sizeof(WORD),
		D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC,
		D3DFMT_INDEX16,
		D3DPOOL_DEFAULT,
		&shadowDecalIndexBufferD3D
	)))
		return FALSE;

	if (shadowDecalVertexBufferD3D == nullptr)
	{	// Create vertex buffer

		if (FAILED(m_pDev->CreateVertexBuffer
		(
			SHADOW_DECAL_VERTEX_SIZE*sizeof(SHADOW_DECAL_VERTEX),
			D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC,
			0,
			D3DPOOL_DEFAULT,
			&shadowDecalVertexBufferD3D
		)))
			return FALSE;
	}

	return TRUE;
}

void W3DProjectedShadowManager::ReleaseResources()
{
	invalidateCachedLightPositions();	//textures need to be updated
	REF_PTR_RELEASE(m_dynamicRenderTarget);	//need to create a new render target
	if (shadowDecalIndexBufferD3D)
		shadowDecalIndexBufferD3D->Release();
	if (shadowDecalVertexBufferD3D)
		shadowDecalVertexBufferD3D->Release();
	shadowDecalIndexBufferD3D=nullptr;
	shadowDecalVertexBufferD3D=nullptr;
}

void W3DProjectedShadowManager::invalidateCachedLightPositions()
{
	m_W3DShadowTextureManager->invalidateCachedLightPositions();
}

void W3DProjectedShadowManager::updateRenderTargetTextures()
{
	///@todo: Don't update texture for shadows that can't be seen!!

	W3DProjectedShadow *shadow;
	if (!m_shadowList)
		return;	//there are no shadows to render.

	if (!TheGlobalData->m_useShadowDecals)
		return;

	if (m_numProjectionShadows)
	for( shadow = m_shadowList; shadow; shadow = shadow->m_next )
	{	//decals don't need any updates on a per-frame basis since
		//the image never changes.
		// GeneralsX @bugfix Copilot 11/05/2026 ShadowType is bitmask; update only projection shadows.
		if (shadow->m_type & (SHADOW_PROJECTION | SHADOW_DYNAMIC_PROJECTION))
			shadow->update();
	}
}

///Renders shadow on part of terrain covered by world-space bounding box.
Int W3DProjectedShadowManager::renderProjectedTerrainShadow(W3DProjectedShadow *shadow, AABoxClass &box)
{
	static	Matrix4x4 mWorld(true);	//initialize to identity matrix
	struct SHADOW_VOLUME_VERTEX	//vertex structure passed to D3D
	{
		float x,y,z;
	};

	Int i,j,k;
	UnsignedByte alpha[4];
	float UA[4], VA[4];
	Bool flipForBlend;


	#define SHADOW_VOLUME_FVF	D3DFVF_XYZ

	if (TheTerrainRenderObject)
	{
		WorldHeightMap *hmap=TheTerrainRenderObject->getMap();

		//Find size of heightmap sub-rectangle affected by shadow
		Real cx=box.Center.X;
		Real cy=box.Center.Y;
		Real dx=box.Extent.X;
		Real dy=box.Extent.Y;
		Real mapScaleInv=1.0f/MAP_XY_FACTOR;
		SHADOW_VOLUME_VERTEX* pvVertices;
		UnsignedShort *pvIndices;
		LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();

		if (!m_pDev)	return 0;

		//Get terrain cell index for area with shadow
		Int startX=REAL_TO_INT_FLOOR(((cx - dx)*mapScaleInv));
		Int endX=REAL_TO_INT_CEIL(((cx + dx)*mapScaleInv));
		Int	startY=REAL_TO_INT_FLOOR(((cy - dy)*mapScaleInv));
		Int endY=REAL_TO_INT_CEIL(((cy + dy)*mapScaleInv));

		// GeneralsX @bugfix Copilot 11/05/2026 Use MAX/MIN portability macros for shadow bounds clipping.
		startX = MAX(startX,0);
		endX = MIN(endX,hmap->getXExtent()-1);
		startY = MAX(startY,0);
		endY = MIN(endY,hmap->getYExtent()-1);

		Int vertsPerRow=endX - startX+1;	//number of cells +1
		Int vertsPerColumn=endY-startY+1;	//number of cells +1

		if (vertsPerRow == 1 || vertsPerColumn == 1)
			return 0;	//nothing to render

		Int numVerts = vertsPerRow *vertsPerColumn;	//number of terrain vertices

		if (nShadowVertsInBuf > (SHADOW_VERTEX_SIZE-numVerts))	//check if room for model verts
		{	//flush the buffer by drawing the contents and re-locking again
			if (shadowVertexBufferD3D->Lock(0,numVerts*sizeof(SHADOW_VOLUME_VERTEX),(unsigned char**)&pvVertices,D3DLOCK_DISCARD) != D3D_OK)
				return 0;
			nShadowVertsInBuf=0;
			nShadowStartBatchVertex=0;
		}
		else
		{	if (shadowVertexBufferD3D->Lock(nShadowVertsInBuf*sizeof(SHADOW_VOLUME_VERTEX),numVerts*sizeof(SHADOW_VOLUME_VERTEX), (unsigned char**)&pvVertices,D3DLOCK_NOOVERWRITE) != D3D_OK)
				return 0;
		}

		if(pvVertices)
		{
			//insert each cell's bottom/left edge vertex
			for (j=startY; j <= endY; j++)
			{
				float ycoord = (float)j * MAP_XY_FACTOR;

				for (i=startX; i <= endX; i++)
				{
					pvVertices->x=(float)i*MAP_XY_FACTOR;
					pvVertices->y=ycoord;
					pvVertices->z=(float)hmap->getHeight(i,j)*MAP_HEIGHT_SCALE;
					pvVertices++;
				}
			}
		}

		shadowVertexBufferD3D->Unlock();

		Int numIndex=(endX - startX) * (endY-startY)*6;	//6 indices per terrain cell (2 triangles).

		if (nShadowIndicesInBuf > (SHADOW_INDEX_SIZE-numIndex))	//check if room for model verts
		{	//flush the buffer by drawing the contents and re-locking again
			if (shadowIndexBufferD3D->Lock(0,numIndex*sizeof(short),(unsigned char**)&pvIndices,D3DLOCK_DISCARD) != D3D_OK)
				return 0;
			nShadowIndicesInBuf=0;
			nShadowStartBatchIndex=0;
		}
		else
		{	if (shadowIndexBufferD3D->Lock(nShadowIndicesInBuf*sizeof(short),numIndex*sizeof(short), (unsigned char**)&pvIndices,D3DLOCK_NOOVERWRITE) != D3D_OK)
				return 0;
		}

		if(pvIndices)
		{		//fill each cell's vertex indices
				Int rowStart;
				for (j=startY,rowStart=0; j<endY; j++,rowStart+=vertsPerRow)
				{
					for (i=rowStart,k=startX; k<endX; i++,k++)
					{	///@todo: Fix this to deal with flipped triangles
						hmap->getAlphaUVData(k, j, UA, VA, alpha, &flipForBlend);
/*						if (flipForBlend)
						{	pvIndices[0]=i;
							pvIndices[1]=i+1;
							pvIndices[2]=i+vertsPerRow;
							pvIndices[3]=i+vertsPerRow;
							pvIndices[4]=i+1;
							pvIndices[5]=i+vertsPerRow+1;
						}
						else
						{	pvIndices[0]=i+vertsPerRow;
							pvIndices[4]=pvIndices[1]=i;
							pvIndices[3]=pvIndices[2]=i+vertsPerRow+1;
							pvIndices[5]=i+1;
						}*/
						///@todo: fix the winding order in heightmap to be in strip order like above!
						if (flipForBlend)
						{	pvIndices[0]=i+1;
							pvIndices[1]=i+vertsPerRow;
							pvIndices[2]=i;
							pvIndices[3]=i+1;
							pvIndices[4]=i+1+vertsPerRow;
							pvIndices[5]=i+vertsPerRow;
						}
						else
						{	pvIndices[0]=i;
							pvIndices[1]=i+1+vertsPerRow;
							pvIndices[2]=i+vertsPerRow;
							pvIndices[3]=i;
							pvIndices[4]=i+1;
							pvIndices[5]=i+1+vertsPerRow;
						}
						pvIndices += 6;
					}
				}
		}

		shadowIndexBufferD3D->Unlock();

		m_pDev->SetIndices(shadowIndexBufferD3D,nShadowStartBatchVertex);

		m_pDev->SetTransform(D3DTS_WORLD,(_D3DMATRIX *)&mWorld);

		m_pDev->SetStreamSource(0,shadowVertexBufferD3D,sizeof(SHADOW_VOLUME_VERTEX));
		m_pDev->SetVertexShader(SHADOW_VOLUME_FVF);

		Int numPolys = (endX - startX)*(endY - startY)*2;	//2 triangles per cell

		m_pDev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);	//should reject background pixels
		m_pDev->SetRenderState( D3DRS_STENCILENABLE, TRUE );
		m_pDev->SetRenderState( D3DRS_STENCILFUNC,     D3DCMP_ALWAYS );
		m_pDev->SetRenderState( D3DRS_STENCILREF,      0x1 );
		m_pDev->SetRenderState( D3DRS_STENCILMASK,     0xffffffff );
		m_pDev->SetRenderState( D3DRS_STENCILWRITEMASK,0xffffffff );
		m_pDev->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
		m_pDev->SetRenderState( D3DRS_STENCILFAIL,  D3DSTENCILOP_KEEP );
		m_pDev->SetRenderState( D3DRS_STENCILPASS,  D3DSTENCILOP_INCR );

//    m_pDev->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );	//useful to see bounds
		m_pDev->SetRenderState( D3DRS_LIGHTING, FALSE);
		m_pDev->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_DESTCOLOR);
		m_pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );


		if (DX8Wrapper::_Is_Triangle_Draw_Enabled())
		{
			Debug_Statistics::Record_DX8_Polys_And_Vertices(numPolys,numVerts,ShaderClass::_PresetOpaqueShader);
			m_pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,numVerts,nShadowStartBatchIndex,numPolys);
		}

		m_pDev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);	//should reject background pixels
		m_pDev->SetRenderState( D3DRS_STENCILENABLE, FALSE );
//    m_pDev->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
		m_pDev->SetRenderState( D3DRS_LIGHTING, TRUE);

		nShadowVertsInBuf += numVerts;
		nShadowStartBatchVertex=nShadowVertsInBuf;

		nShadowIndicesInBuf += numIndex;
		nShadowStartBatchIndex=nShadowIndicesInBuf;
		return 1;
	}
	return 0;
}

#if 0

TextureClass *snow=nullptr;
TextureClass *grass=nullptr;
TextureClass *ground=nullptr;

#define V_COUNT  (4*4)	//4 vertices per cell
#define I_COUNT  (4*6)	//6 indices per cell
#define TILE_HEIGHT	10.1f
#define TILE_DIFFUSE 0x00b4b0a5

enum BlendDirection CPP_11(: Int)
{	B_A,	//visible on all sides
	B_R,	//visible on right
	B_L,	//visible on left
	B_T,	//visible on top
	B_B,	//visble on bottom
	B_TL,	//visible on top/left
	B_BR,	//visible on bottom/right
	B_TR,	//visible on top/right
	B_BL	//visilbe on bottom/left
};

//Vertex alpha values for each blend direction assuming tile vertices
//start at top left corner and continue counter-clockwise
DWORD BDToVA[9][4]=
{
	{0xff000000,0xff000000,0xff000000,0xff000000},
	{0,0,0xff000000,0xff000000},
	{0xff000000,0xff000000,0,0},
	{0xff000000,0,0,0xff000000},
	{0,0xff000000,0xff000000,0},
	{0xff000000,0,0,0},
	{0,0,0xff000000,0},
	{0,0,0,0xff000000},
	{0,0xff000000,0,0}
};

static void RenderVBTile(TextureClass *text, Real ox, Real oy, Real ou, Real ov, BlendDirection bd=B_A)
{
	DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC_DX8,DX8_FVF_XYZNDUV2,4);
	DynamicIBAccessClass ib_access(BUFFER_TYPE_DYNAMIC_DX8,6);

	DynamicVBAccessClass::WriteLockClass lock(&vb_access);
	VertexFormatXYZNDUV2* vb= lock.Get_Formatted_Vertex_Array();
	DynamicIBAccessClass::WriteLockClass lockib(&ib_access);
	if (!vb) return;

	UnsignedShort *ib=lockib.Get_Index_Array();

	vb->x=ox;
	vb->y=oy;
	vb->z=TILE_HEIGHT;
	vb->diffuse=TILE_DIFFUSE|BDToVA[bd][0];
	vb->u1=ou;
	vb->v1=ov;
	vb++;

	vb->x=ox;
	vb->y=oy-10.0f;
	vb->z=TILE_HEIGHT;
	vb->diffuse=TILE_DIFFUSE|BDToVA[bd][1];
	vb->u1=ou;
	vb->v1=ov+0.25f;
	vb++;

	vb->x=ox+10.0f;
	vb->y=oy-10.0f;
	vb->z=TILE_HEIGHT;
	vb->diffuse=TILE_DIFFUSE|BDToVA[bd][2];
	vb->u1=ou+0.25f;
	vb->v1=ov+0.25f;
	vb++;

	vb->x=ox+10.0f;
	vb->y=oy;
	vb->z=TILE_HEIGHT;
	vb->diffuse=TILE_DIFFUSE|BDToVA[bd][3];
	vb->u1=ou+0.25f;
	vb->v1=ov;
	vb++;

	ib[0]=0;
	ib[1]=1;
	ib[2]=3;
	ib[3]=3;
	ib[4]=1;
	ib[5]=2;

	if (bd == B_TR || bd == B_BL)
	{	//need to flip triangles so alpha gradient doesn't follow diagonal edge
		ib[2]=2;
		ib[4]=0;
	}

	DX8Wrapper::Set_Index_Buffer(ib_access,0);
	DX8Wrapper::Set_Vertex_Buffer(vb_access);
	DX8Wrapper::Set_Texture(0, text);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA  );
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE );
	ShaderClass::Invalidate();	//invalidate to force shader to reset since we directly changed states
	DX8Wrapper::Draw_Triangles(	0,2, 0,	4);	//draw a quad, 2 triangles, 4 verts
}

//Debug code used to draw some dummy polygons.
void TestBlendRender(RenderInfoClass & rinfo)
{
	static Int doInit=1;

	if (doInit)
	{	doInit = 0;
		snow = WW3DAssetManager::Get_Instance()->Get_Texture("TXSnow04a.tga");
		grass = WW3DAssetManager::Get_Instance()->Get_Texture("TMGras23a.tga");
		ground = WW3DAssetManager::Get_Instance()->Get_Texture("TXAsph01a.tga");
	}

	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);
	DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader);

	Matrix3D tm(1);	//identity
	DX8Wrapper::Set_Transform(D3DTS_WORLD,tm);

	//grass
	RenderVBTile(grass,580.0f,480.0f,0.0f,0.0f);	RenderVBTile(grass,590.0f,480.0f,0.25f,0.0f);
	RenderVBTile(grass,580.0f,470.0f,0.0f,0.25f);	RenderVBTile(grass,590.0f,470.0f,0.25f,0.25f);
	RenderVBTile(grass,580.0f,460.0f,0.0f,0.5f);	RenderVBTile(grass,590.0f,460.0f,0.25f,0.5f,B_L);
	RenderVBTile(grass,580.0f,450.0f,0.0f,0.75f);	RenderVBTile(grass,590.0f,450.0f,0.25f,0.75f,B_L);
	RenderVBTile(grass,580.0f,440.0f,0.0f,0.0f);	RenderVBTile(grass,590.0f,440.0f,0.25f,0.0f,B_L);

	RenderVBTile(grass,610.0f,460.0f,0.0f,0.5f, B_B);
	RenderVBTile(grass,610.0f,450.0f,0.0f,0.75f);
	RenderVBTile(grass,610.0f,440.0f,0.0f,0.0f);


	//snow
	RenderVBTile(snow,590.0f,480.0f,0.0f,0.0f, B_R);	RenderVBTile(snow,600.0f,480.0f,0.25f,0.0f);	RenderVBTile(snow,610.0f,480.0f,0.5f,0.0f);
	RenderVBTile(snow,590.0f,470.0f,0.0f,0.25f, B_R);	RenderVBTile(snow,600.0f,470.0f,0.25f,0.25f);	RenderVBTile(snow,610.0f,470.0f,0.5f,0.25f);
	RenderVBTile(snow,590.0f,460.0f,0.0f,0.5f, B_TR);	RenderVBTile(snow,600.0f,460.0f,0.25f,0.5f,B_T); RenderVBTile(snow,610.0f,460.0f,0.5f,0.5f,B_T);
}
#endif

void W3DProjectedShadowManager::flushDecals(W3DShadowTexture *texture, ShadowType type)
{
	static	Matrix4x4 mWorld(true);	//initialize to identity matrix

	if (nShadowDecalVertsInBatch == 0 && nShadowDecalPolysInBatch == 0)
	{	//nothing to render
		return;
	}

	LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();
	if (!m_pDev)	return;	//no D3D Device to render

	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);
	DX8Wrapper::Set_Texture(0,texture->getTexture());

//	DX8Wrapper::Set_Shader(ShaderClass::_PresetOpaqueShader);	//good for debugging, draws without alpha
	// GeneralsX @bugfix Meeseeks 17/06/2026 Support combined decal flags by replacing strict switch with bitwise checks.
	if (type & SHADOW_ALPHA_DECAL)
	{
		DX8Wrapper::Set_Shader(ShaderClass::_PresetAlphaShader);
	}
	else if (type & SHADOW_ADDITIVE_DECAL)
	{
		DX8Wrapper::Set_Shader(ShaderClass::_PresetAdditiveShader);
	}
	else if (type & SHADOW_DECAL)
	{
		DX8Wrapper::Set_Shader(ShaderClass::_PresetMultiplicativeShader);
	}

//	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAREF,0x60);
//	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAFUNC,D3DCMP_GREATEREQUAL);
	//_PresetAlphaSpriteShader

	DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

//Alpha Blended Shadows
//	m_pDev->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
//	m_pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA  );
/*	UnsignedInt color=TheW3DShadowManager->getShadowColor();
	m_pDev->SetRenderState( D3DRS_TEXTUREFACTOR, 0xff000000 | color);
	m_pDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	m_pDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	m_pDev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

	m_pDev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	m_pDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	m_pDev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
*/


	m_pDev->SetIndices(shadowDecalIndexBufferD3D,nShadowDecalStartBatchVertex);
	m_pDev->SetTransform(D3DTS_WORLD,(_D3DMATRIX *)&mWorld);

	m_pDev->SetStreamSource(0,shadowDecalVertexBufferD3D,sizeof(SHADOW_DECAL_VERTEX));
	m_pDev->SetVertexShader(SHADOW_DECAL_FVF);

//Hard Shadows using stencil
/*	m_pDev->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_ZERO);
	m_pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
	m_pDev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);	//should reject background pixels
	m_pDev->SetRenderState( D3DRS_STENCILENABLE, TRUE );
*/
/*	m_pDev->SetRenderState( D3DRS_STENCILFUNC,     D3DCMP_ALWAYS );
	m_pDev->SetRenderState( D3DRS_STENCILREF,      0x1 );
	m_pDev->SetRenderState( D3DRS_STENCILMASK,     0xffffffff );
	m_pDev->SetRenderState( D3DRS_STENCILWRITEMASK,0xffffffff );
	m_pDev->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
	m_pDev->SetRenderState( D3DRS_STENCILFAIL,  D3DSTENCILOP_KEEP );
	m_pDev->SetRenderState( D3DRS_STENCILPASS,  D3DSTENCILOP_INCR );
*/
//m_pDev->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );	//useful to see bounds

	if (DX8Wrapper::_Is_Triangle_Draw_Enabled())
	{
		Debug_Statistics::Record_DX8_Polys_And_Vertices(nShadowDecalPolysInBatch,nShadowDecalVertsInBatch,ShaderClass::_PresetOpaqueShader);
		m_pDev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,nShadowDecalVertsInBatch,nShadowDecalStartBatchIndex,nShadowDecalPolysInBatch);
	}

//	m_pDev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);	//should reject background pixels
//	m_pDev->SetRenderState( D3DRS_STENCILENABLE, FALSE );
//m_pDev->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );


	//Restore multiplicative sprite shader
//	m_pDev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_SRCCOLOR);	//restore W3D state
//	m_pDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);

/*	m_pDev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	m_pDev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	m_pDev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
	m_pDev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	m_pDev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	m_pDev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
*/
	nShadowDecalStartBatchVertex=nShadowDecalVertsInBuf;
	nShadowDecalStartBatchIndex=nShadowDecalIndicesInBuf;
	nShadowDecalPolysInBatch=0;	//reset number of polys in texture batch
	nShadowDecalVertsInBatch=0;
}

/*
void testShadowDecal()
{
	Shadow::ShadowTypeInfo decalInfo;
	decalInfo.allowUpdates = FALSE;	//shadow image will never update
	decalInfo.allowWorldAlign = TRUE;	//shadow image will wrap around world objects
	decalInfo.m_type = SHADOW_ALPHA_DECAL;
	strcpy(decalInfo.m_ShadowName,"exwave256");
	decalInfo.m_sizeX = 1280.0f;
	decalInfo.m_sizeY = 1280.0f;
	decalInfo.m_offsetX = 0;
	decalInfo.m_offsetY = 0;
	Shadow *shadow=TheProjectedShadowManager->addDecal(&decalInfo);
	shadow->setPosition(600,600,600);
	shadow->setAngle(0.0f);
	shadow->setColor(0xffff0000);
}
*/

#define BRIDGE_OFFSET_FACTOR 1.5f
/**Decals have a low poly count so its better to render large numbers at once.  This system will queue them
up until the buffers fill up.  It will then flush the buffer (draw decals) and be ready for new decals.  This
is an optimized system that only uses the render objects bounding box to determine shadow visibility.
*/
void W3DProjectedShadowManager::queueDecal(W3DProjectedShadow *shadow)
{
	int i,j,k;
	Vector3 hmapVertex,objPos;
	AABoxClass box;
	Matrix3D   objXform(1);
	Real cx,cy,dx,dy;
	Real mapScaleInv=1.0f/MAP_XY_FACTOR;
	static Vector3 objCenter(0,0,0);
	Vector3 uVector,vVector;
	Real uOffset,vOffset,vecLength;
	Int borderSize;
	RenderObjClass *robj=shadow->m_robj;
	Real layerHeight=0;

	if (TheTerrainRenderObject)
	{
		LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();

		if (!m_pDev)	return;	//no D3D Device to render

		WorldHeightMap *hmap=TheTerrainRenderObject->getMap();
		borderSize=hmap->getBorderSize();
		if (robj)
		{
			objPos=robj->Get_Position();
			objXform=robj->Get_Transform();
			if (robj->Get_User_Data())
			{
				Drawable *draw=((DrawableInfo *)robj->Get_User_Data())->m_drawable;
				const Object *object=draw->getObject();
				PathfindLayerEnum objectLayer;
				if (object && (objectLayer=object->getLayer()) != LAYER_GROUND)
				{	//check if object that this decal belongs to is not on the ground (bridge?)
					layerHeight=BRIDGE_OFFSET_FACTOR+TheTerrainLogic->getLayerHeight(objPos.X,objPos.Y,objectLayer);
				}
			}
		}
		else
		{	//no render object so use shadow's local position and default orientation
			objPos.Set(shadow->m_x,shadow->m_y,shadow->m_z);
			objXform.Rotate_Z(shadow->m_localAngle);
		}

		//Find size of heightmap sub-rectangle affected by shadow
		//If user supplied size values, ignore bounding box

		objPos.Z=0.0f;	//we don't care about object height since shadows project top-down

		uVector=objXform.Get_X_Vector();

		uVector.Z=0.0f;
		vecLength=uVector.Length();
		if (vecLength != 0.0f)	//prevent divide by zero
		{	uVector *= 1.0f/vecLength;
			vVector = uVector;
			vVector.Rotate_Z(-1.0f,0.0f);	//rotate u vector by -90 degress to get v vector.
		}
		else
		{
			vVector=objXform.Get_Y_Vector();
			vVector.Z=0.0f;
			vecLength=vVector.Length();
			if (vecLength != 0.0f)	//prevent divide by zero
				vVector *= 1.0f/vecLength;
			else
				vVector.Set(0.0f,-1.0f,0.0f); //Point uvector in default direction

			uVector = vVector;
			uVector.Rotate_Z(1.0f,0.0f);	//rotate v vector by 90 degress to get u vector.
		}

		//Compute bounding box of projection
		Vector3 boxCorners[4];	//top-left, top-right, bottom-right, bottom-left
		dx = shadow->m_decalSizeX;
		dy = shadow->m_decalSizeY;
		Vector3 left_x=-dx * (uVector * (0.5f + shadow->m_decalOffsetU));
		Vector3 right_x = dx * (uVector * (0.5f - shadow->m_decalOffsetU));
		Vector3 top_y = -dy * (vVector * (0.5f + shadow->m_decalOffsetV));
		Vector3 bottom_y = dy * (vVector * (0.5f - shadow->m_decalOffsetV));
		///@todo: Optimize this bounding box calculation to use transformed extents
		//Also skip bounding box calculation if object has not moved.
		boxCorners[0] = left_x + top_y;
		boxCorners[1] = right_x + top_y;
		boxCorners[2] = right_x + bottom_y;
		boxCorners[3] = left_x + bottom_y;

		Real min_x,max_x,min_y,max_y;
		max_x=min_x=boxCorners[0].X;
		max_y=min_y=boxCorners[0].Y;

		// GeneralsX @bugfix Copilot 11/05/2026 Use MAX/MIN portability macros for projection bounds.
		for (Int bi=1; bi<4; bi++)
		{	max_x = MAX(max_x,boxCorners[bi].X);
			min_x = MIN(min_x,boxCorners[bi].X);
			max_y = MAX(max_y,boxCorners[bi].Y);
			min_y = MIN(min_y,boxCorners[bi].Y);
		}

		uVector *= shadow->m_oowDecalSizeX;
		vVector *= shadow->m_oowDecalSizeY;
		uOffset = shadow->m_decalOffsetU + 0.5f;
		vOffset = shadow->m_decalOffsetV + 0.5f;

/*		{	//This version will stretch to fit orientation of object
			///@todo: Most of the values below can be cached in shadow object
			shadow->m_robj->Get_Obj_Space_Bounding_Box(box);
			decalSizeX=box.Extent.X*2.0f;	//use local space bounding box to determine shadow size
			decalSizeY=box.Extent.Y*2.0f;
//			box=shadow->m_robj->Get_Bounding_Box();	//get world-space bounding box
			objPos = box.Center;
			box.Init(objCenter,Vector3(decalSizeX*0.5f,decalSizeY*0.5f,1.0f));
			box.Transform(objXform);	//transform box from object space to world space
			box.Translate(objPos=objXform.Rotate_Vector(objPos));
			objPos += shadow->m_robj->Get_Position();
		}
*/
//	  Experimental code to try and get a better fitting bounding box around shadow
	/*  Experimental code to try and get a better fitting bounding box around shadow
	{	//use the object's bounding box to determine shadow extent
		///@todo: Most of the values below can be cached in shadow object
		uVector=objXform.Get_X_Vector();
		uVector.Z=0;
		uVector.Normalize();

		vVector=objXform.Get_Y_Vector();	//invert direction since v axis runs right relative to u.
		vVector.Z=0;
		vVector.Normalize();

		shadow->m_robj->Get_Obj_Space_Bounding_Box(box);
		decalSizeX = box.Extent.X * 2.0f;
		decalSizeY = box.Extent.Y * 2.0f;

		Real newExtentX = fabs(uVector * box.Extent) + fabs(uVector * box.Center);	//get new extent for object orientation
		Real newExtentY = fabs(vVector * box.Extent) + fabs(vVector * box.Center);  //get new extent for object orientation

		objPos += uVector * box.Center.X;
		objPos += vVector * box.Center.Y;
//			objPos.Y = objPos.Y + vVector * box.Center;
		objPos.Z = 0.0f;

		//set new oriented bounding box extents
		box.Extent.Set(newExtentX, newExtentY, 0);
		box.Center.Set(objPos.X,objPos.Y,objPos.Z);
	}
*/

		cx=box.Center.X;
		cy=box.Center.Y;
		dx=box.Extent.X;
		dy=box.Extent.Y;

		//Get terrain cell index for area with shadow
		Int startX=REAL_TO_INT_FLOOR(((objPos.X+min_x)*mapScaleInv)) + borderSize;
		Int endX=REAL_TO_INT_CEIL(((objPos.X+max_x)*mapScaleInv)) + borderSize;
		Int	startY=REAL_TO_INT_FLOOR(((objPos.Y+min_y)*mapScaleInv)) + borderSize;
		Int endY=REAL_TO_INT_CEIL(((objPos.Y+max_y)*mapScaleInv)) + borderSize;

		// GeneralsX @bugfix Copilot 11/05/2026 Use MAX/MIN portability macros for draw-window clipping.
		startX = MAX(startX,m_drawStartX);
		startX = MIN(startX,m_drawEdgeX);
		startY = MAX(startY,m_drawStartY);
		startY = MIN(startY,m_drawEdgeY);

		endX = MAX(endX,m_drawStartX);
		endX = MIN(endX,m_drawEdgeX);
		endY = MAX(endY,m_drawStartY);
		endY = MIN(endY,m_drawEdgeY);

		//Check if decal too large to fit inside 65536 index buffer.
		//try clipping each direction to < 104 since that's more than
		//enough to cover typical map.
		Int numExtraX=(endX - startX+1)-104;
		if (numExtraX > 0)
		{	//figure out how much to clip out at each edge of decal
			Int numStartExtraX=REAL_TO_INT_FLOOR((float)numExtraX/2.0f);
			Int numEdgeExtraX=numExtraX-numStartExtraX;
			startX+=numStartExtraX;
			endX-=numEdgeExtraX;
		}
		Int numExtraY=(endY - startY+1)-104;
		if (numExtraY > 0)
		{
			Int numStartExtraY=REAL_TO_INT_FLOOR((float)numExtraY/2.0f);
			Int numEdgeExtraY=numExtraY-numStartExtraY;

			startY+=numStartExtraY;
			endY-=numEdgeExtraY;
		}

		Int vertsPerRow=endX - startX+1;	//number of cells +1
		Int vertsPerColumn=endY-startY+1;	//number of cells +1

		if (vertsPerRow <= 1 || vertsPerColumn <= 1)
			return;	//nothing to render

		Int numVerts = vertsPerRow *vertsPerColumn;	//number of terrain vertices
		Int numIndex=(endX - startX) * (endY-startY)*6;	//6 indices per terrain cell (2 triangles).

		SHADOW_DECAL_VERTEX* pvVertices;
		UnsignedShort *pvIndices;

		if (nShadowDecalVertsInBuf > (SHADOW_DECAL_VERTEX_SIZE-numVerts))	//check if room for model verts
		{	//flush the buffer by drawing the contents and re-locking again
			flushDecals(shadow->m_shadowTexture[0], shadow->m_type);
			if (shadowDecalVertexBufferD3D->Lock(0,numVerts*sizeof(SHADOW_DECAL_VERTEX),(unsigned char**)&pvVertices,D3DLOCK_DISCARD) != D3D_OK)
				return;

			nShadowDecalStartBatchVertex=0;
			nShadowDecalPolysInBatch=0;	//reset number of polys in texture batch
			nShadowDecalVertsInBatch=0;
			nShadowDecalVertsInBuf=0;
		}
		else
		{	if (shadowDecalVertexBufferD3D->Lock(nShadowDecalVertsInBuf*sizeof(SHADOW_DECAL_VERTEX),numVerts*sizeof(SHADOW_DECAL_VERTEX), (unsigned char**)&pvVertices,D3DLOCK_NOOVERWRITE) != D3D_OK)
				return;
		}

		//code to deal with rotated shadows based on sun direction, fix this later.  For now shadow rotates with object rotation.
		//shadow->m_shadowTexture[0]->getDecalUVAxis(&uVector,&vVector);
		//uVector *= 20.0f/dx;//shadow->m_decalRadius;	//scale texture to fit object
		//vVector *= 20.0f/dy;//1.2f;//shadow->m_decalRadius;	//scale texture to fit object
/*		uVector=objXform.Get_X_Vector();
		uVector.Normalize();
		uVector /= decalSizeX + (1.0f+4.0f/64.0f);	//texture has 1 pixel transparent border so we strtech up to make sure solid pixels reach extent..
		vVector=objXform.Get_Y_Vector() * -1.0f;	//invert direction since v axis runs right relative to u.
		vVector.Normalize();
		vVector /= decalSizeY + (1.0f+4.0f/64.0f);
*/
		DEBUG_ASSERTCRASH(numVerts == ((endY-startY+1)*(endX-startX+1)), ("queueDecal VB size mismatch"));

		if(pvVertices)
		{
			if (layerHeight)
				for (j=startY; j <= endY; j++)
				{
					hmapVertex.Y=(float)(j-borderSize) * MAP_XY_FACTOR;

					for (i=startX; i <= endX; i++)
					{
						hmapVertex.X=(float)(i-borderSize)*MAP_XY_FACTOR;
						// GeneralsX @bugfix Copilot 11/05/2026 Use MAX portability macro when clamping bridge-layer shadow height.
						hmapVertex.Z=MAX((float)hmap->getHeight(i,j)*MAP_HEIGHT_SCALE,layerHeight);
						pvVertices->x=hmapVertex.X;
						pvVertices->y=hmapVertex.Y;
						pvVertices->z=hmapVertex.Z;
						pvVertices->diffuse=shadow->m_diffuse;
						pvVertices->u=Vector3::Dot_Product(uVector, (hmapVertex-objPos))+uOffset;
						pvVertices->v=Vector3::Dot_Product(vVector, (hmapVertex-objPos))+vOffset;
						pvVertices++;
					}
				}
			else
			//insert each cell's bottom/left edge vertex
			for (j=startY; j <= endY; j++)
			{
				hmapVertex.Y=(float)(j-borderSize) * MAP_XY_FACTOR;

				for (i=startX; i <= endX; i++)
				{
					hmapVertex.X=(float)(i-borderSize)*MAP_XY_FACTOR;
					hmapVertex.Z=(float)hmap->getHeight(i,j)*MAP_HEIGHT_SCALE+0.01f * MAP_XY_FACTOR;
					pvVertices->x=hmapVertex.X;
					pvVertices->y=hmapVertex.Y;
					pvVertices->z=hmapVertex.Z;
					pvVertices->diffuse=shadow->m_diffuse;
					pvVertices->u=Vector3::Dot_Product(uVector, (hmapVertex-objPos))+uOffset;
					pvVertices->v=Vector3::Dot_Product(vVector, (hmapVertex-objPos))+vOffset;
					pvVertices++;
				}
			}
		}

		shadowDecalVertexBufferD3D->Unlock();

		if (nShadowDecalIndicesInBuf > (SHADOW_DECAL_INDEX_SIZE-numIndex))	//check if room for model verts
		{	//flush the buffer by drawing the contents and re-locking again
			flushDecals(shadow->m_shadowTexture[0], shadow->m_type);

			if (shadowDecalIndexBufferD3D->Lock(0,numIndex*sizeof(short),(unsigned char**)&pvIndices,D3DLOCK_DISCARD) != D3D_OK)
				return;

			nShadowDecalStartBatchIndex=0;
			nShadowDecalPolysInBatch=0;	//reset number of polys in texture batch
			nShadowDecalVertsInBatch=0;
			nShadowDecalIndicesInBuf=0;
		}
		else
		{	if (shadowDecalIndexBufferD3D->Lock(nShadowDecalIndicesInBuf*sizeof(short),numIndex*sizeof(short), (unsigned char**)&pvIndices,D3DLOCK_NOOVERWRITE) != D3D_OK)
				return;
		}

		if(pvIndices)
		{	//fill each cell's vertex indices
			Int rowStart;
			for (j=startY,rowStart=0; j<endY; j++,rowStart+=vertsPerRow)
			{
				for (i=rowStart,k=startX; k<endX; i++,k++)
				{
					///@todo: fix the winding order in heightmap to be in strip order like above!
					if (hmap->getFlipState(k,j))
					{	pvIndices[0]=i+1+nShadowDecalVertsInBatch;
						pvIndices[1]=i+vertsPerRow+nShadowDecalVertsInBatch;
						pvIndices[2]=i+nShadowDecalVertsInBatch;
						pvIndices[3]=i+1+nShadowDecalVertsInBatch;
						pvIndices[4]=i+1+vertsPerRow+nShadowDecalVertsInBatch;
						pvIndices[5]=i+vertsPerRow+nShadowDecalVertsInBatch;
					}
					else
					{	pvIndices[0]=i+nShadowDecalVertsInBatch;
						pvIndices[1]=i+1+vertsPerRow+nShadowDecalVertsInBatch;
						pvIndices[2]=i+vertsPerRow+nShadowDecalVertsInBatch;
						pvIndices[3]=i+nShadowDecalVertsInBatch;
						pvIndices[4]=i+1+nShadowDecalVertsInBatch;
						pvIndices[5]=i+1+vertsPerRow+nShadowDecalVertsInBatch;
					}
					pvIndices += 6;
				}
			}
		}

		shadowDecalIndexBufferD3D->Unlock();

		Int numPolys = (endX - startX)*(endY - startY)*2;	//2 triangles per cell
		nShadowDecalPolysInBatch += numPolys;

		nShadowDecalVertsInBuf += numVerts;
		nShadowDecalVertsInBatch += numVerts;
//		nShadowDecalStartBatchVertex=nShadowDecalVertsInBuf;

		nShadowDecalIndicesInBuf += numIndex;
//		nShadowDecalStartBatchIndex=nShadowDecalIndicesInBuf;
		return;
	}

}

/**Simpler/faster decal system that always uses 2 triangles that are roughly oriented
to terrain.  Since they are not projected onto terrain, there may be clipping
artifacts in certain situations.
TODO: Too much clipping.  Need to check terrain heights at all 4 corners and adjust tilt to match*/
///@todo: We should have a pre-made static filled index buffer since we always send down 2 triangles.
void W3DProjectedShadowManager::queueSimpleDecal(W3DProjectedShadow *shadow)
{
	Vector3 objPos;
	Matrix3D   objXform;
	Vector3 uVector,vVector;
	Coord3D normal;

	if (TheTerrainRenderObject)
	{
		LPDIRECT3DDEVICE8 m_pDev=DX8Wrapper::_Get_D3D_Device8();

		if (!m_pDev)	return;	//no D3D Device to render

		objPos=shadow->m_robj->Get_Position();
		objXform=shadow->m_robj->Get_Transform();
		Real groundHeight=TheTerrainRenderObject->getHeightMapHeight(objPos.X, objPos.Y, &normal);
		Vector3 groundNormal(normal.x,normal.y,normal.z);

		//Find new tu vector parallel to terrain by projecting existing x_vector onto
		//terrain normal and subtracting the result.
		uVector=objXform.Get_X_Vector();
		Real uVectorAlongNormal = Vector3::Dot_Product(uVector,groundNormal);
		uVector -= uVectorAlongNormal * groundNormal;
		uVector.Normalize();
		//Find new tv vector parallel to terrain by crossing new tu vector with terrain normal.
		Vector3::Cross_Product(uVector,groundNormal,&vVector);

		Int numVerts = 4;	//number of decal vertices
		Int numIndex=6;	//(2 triangles).

		SHADOW_DECAL_VERTEX* pvVertices;
		UnsignedShort *pvIndices;

		if (nShadowDecalVertsInBuf > (SHADOW_DECAL_VERTEX_SIZE-numVerts))	//check if room for model verts
		{	//flush the buffer by drawing the contents and re-locking again
			flushDecals(shadow->m_shadowTexture[0], shadow->m_type);
			if (shadowDecalVertexBufferD3D->Lock(0,numVerts*sizeof(SHADOW_DECAL_VERTEX),(unsigned char**)&pvVertices,D3DLOCK_DISCARD) != D3D_OK)
				return;

			nShadowDecalStartBatchVertex=0;
			nShadowDecalPolysInBatch=0;	//reset number of polys in texture batch
			nShadowDecalVertsInBatch=0;
			nShadowDecalVertsInBuf=0;
		}
		else
		{	if (shadowDecalVertexBufferD3D->Lock(nShadowDecalVertsInBuf*sizeof(SHADOW_DECAL_VERTEX),numVerts*sizeof(SHADOW_DECAL_VERTEX), (unsigned char**)&pvVertices,D3DLOCK_NOOVERWRITE) != D3D_OK)
				return;
		}

		objPos.Z=groundHeight;	//force decal to ground level
		objPos += groundNormal * 1.0f;	//offset decal slightly above terrain to reduce z-fighting.
		Vector3 vertex;

		if(pvVertices)
		{
			//Top-left
			vertex = objPos + vVector * shadow->m_decalSizeY * -0.5f - uVector * shadow->m_decalSizeX * 0.5f;
			pvVertices->x=vertex.X;
			pvVertices->y=vertex.Y;
			pvVertices->z=vertex.Z;
			pvVertices->u=0.0f;
			pvVertices->v=0.0f;
			pvVertices++;

			//Bottom-left
			vertex += vVector * shadow->m_decalSizeY;
			pvVertices->x=vertex.X;
			pvVertices->y=vertex.Y;
			pvVertices->z=vertex.Z;
			pvVertices->u=0.0f;
			pvVertices->v=1.0f;
			pvVertices++;

			//Bottom-right
			vertex += uVector * shadow->m_decalSizeX;
			pvVertices->x=vertex.X;
			pvVertices->y=vertex.Y;
			pvVertices->z=vertex.Z;
			pvVertices->u=1.0f;
			pvVertices->v=1.0f;
			pvVertices++;

			//Top-right
			vertex -= vVector * shadow->m_decalSizeY;
			pvVertices->x=vertex.X;
			pvVertices->y=vertex.Y;
			pvVertices->z=vertex.Z;
			pvVertices->u=1.0f;
			pvVertices->v=0.0f;
			pvVertices++;
		}

		shadowDecalVertexBufferD3D->Unlock();

		if (nShadowDecalIndicesInBuf > (SHADOW_DECAL_INDEX_SIZE-numIndex))	//check if room for model verts
		{	//flush the buffer by drawing the contents and re-locking again
			flushDecals(shadow->m_shadowTexture[0],shadow->m_type);

			if (shadowDecalIndexBufferD3D->Lock(0,numIndex*sizeof(short),(unsigned char**)&pvIndices,D3DLOCK_DISCARD) != D3D_OK)
				return;

			nShadowDecalStartBatchIndex=0;
			nShadowDecalPolysInBatch=0;	//reset number of polys in texture batch
			nShadowDecalVertsInBatch=0;
			nShadowDecalIndicesInBuf=0;
		}
		else
		{	if (shadowDecalIndexBufferD3D->Lock(nShadowDecalIndicesInBuf*sizeof(short),numIndex*sizeof(short), (unsigned char**)&pvIndices,D3DLOCK_NOOVERWRITE) != D3D_OK)
				return;
		}

		if(pvIndices)
		{	pvIndices[0]=nShadowDecalVertsInBatch;
			pvIndices[1]=nShadowDecalVertsInBatch+1;
			pvIndices[2]=nShadowDecalVertsInBatch+2;
			pvIndices[3]=nShadowDecalVertsInBatch;
			pvIndices[4]=nShadowDecalVertsInBatch+2;
			pvIndices[5]=nShadowDecalVertsInBatch+3;
			pvIndices += 6;
		}

		shadowDecalIndexBufferD3D->Unlock();

		Int numPolys = 2;	//2 triangles per decal
		nShadowDecalPolysInBatch += numPolys;

		nShadowDecalVertsInBuf += numVerts;
		nShadowDecalVertsInBatch += numVerts;

		nShadowDecalIndicesInBuf += numIndex;
		return;
	}

}

void W3DProjectedShadowManager::prepareShadows()
{
	if (!TheTerrainRenderObject)
		return;

	WorldHeightMap *hmap=TheTerrainRenderObject->getMap();

	if (!hmap)
		return;

	//Find extents of visible terrain
	m_drawEdgeY=hmap->getDrawOrgY()+hmap->getDrawHeight()-1;
	m_drawEdgeX=hmap->getDrawOrgX()+hmap->getDrawWidth()-1;
	if (m_drawEdgeX > (hmap->getXExtent()-1))
		m_drawEdgeX = hmap->getXExtent()-1;
	if (m_drawEdgeY > (hmap->getYExtent()-1))
		m_drawEdgeY = hmap->getYExtent()-1;
	m_drawStartX=hmap->getDrawOrgX();
	m_drawStartY=hmap->getDrawOrgY();
}

Int W3DProjectedShadowManager::renderShadows(RenderInfoClass & rinfo)
{
	Int projectionCount=0;

	if (!TheTerrainRenderObject)
		return projectionCount;

	if (!m_shadowList && !m_decalList)
		return	projectionCount;	//there are no shadows to render.

	W3DProjectedShadow *shadow;
	static AABoxClass aaBox;
	static SphereClass sphere;

	//According to Nvidia there's a D3D bug that happens if you don't start with a
	//new dynamic VB each frame - so we force a DISCARD by overflowing the counter.
	nShadowDecalVertsInBuf = 0xffff;
	nShadowDecalIndicesInBuf = 0xffff;

	if (TheGlobalData->m_useShadowDecals)
	{
		// Render the object
		TheDX8MeshRenderer.Set_Camera(&rinfo.Camera);

		//keep track of active decal texture so we can render all decals at once.
		W3DShadowTexture *lastShadowDecalTexture=nullptr;
		ShadowType lastShadowType = SHADOW_NONE;

		for( shadow = m_shadowList; shadow; shadow = shadow->m_next )
		{
			if (shadow->m_isEnabled && !shadow->m_isInvisibleEnabled)
			{
				if (shadow->m_type & SHADOW_DECAL)
				{
					if (lastShadowDecalTexture == nullptr)
						lastShadowDecalTexture=shadow->m_shadowTexture[0];
					if (lastShadowType == SHADOW_NONE)
						lastShadowType = shadow->m_type;

					if (shadow->m_shadowTexture[0] != lastShadowDecalTexture ||
						shadow->m_type != lastShadowType)
					{	flushDecals(lastShadowDecalTexture,lastShadowType);	//switched to a new texture, need to render polys using last texture.
						lastShadowDecalTexture=shadow->m_shadowTexture[0];
						lastShadowType=shadow->m_type;
					}
					///@todo: may need to fix this if shadows are large enough to be seen while object is not visible
					if (shadow->m_robj->Is_Really_Visible())
					{	//queueSimpleDecal(shadow);
						queueDecal(shadow);	//only draw shadow if casting object is visible
						projectionCount++;
					}
					continue;
				}

				//First test if shadow is visible on screen
				sphere=shadow->m_shadowTexture[0]->getBoundingSphere();
				sphere.Center += shadow->m_robj->Get_Position();

				CollisionMath::OverlapType result=CollisionMath::Overlap_Test(*shadowCameraFrustum,sphere);
				if (result == CollisionMath::OVERLAPPED)
				{	//do a more accurate test against bounding box.
					aaBox=shadow->m_shadowTexture[0]->getBoundingBox();
					aaBox.Translate(shadow->m_robj->Get_Position());	//translate bounding box to world space.
					if (CollisionMath::Overlap_Test(*shadowCameraFrustum,aaBox) == CollisionMath::OUTSIDE)
						continue;
				}
				else
				if (result == CollisionMath::OUTSIDE)
					continue;

				//Shadow is visible on screen.  Figure out which visible objects it may affect.

				//Check if bounding sphere was inside so bounding box never initialized
				if (result == CollisionMath::INSIDE)
				{		aaBox=shadow->m_shadowTexture[0]->getBoundingBox();
						aaBox.Translate(shadow->m_robj->Get_Position());	//translate bounding box to world space.
				}

				// GeneralsX @bugfix Meeseeks 17/06/2026 Support combined projection flags by checking bits.
				if (shadow->m_type & SHADOW_PROJECTION)
				{
					//build inverse camera/view transforms needed for projection
					shadow->updateProjectionParameters(rinfo.Camera.Get_Transform());
					TexProjectClass *projector=shadow->getShadowProjector();

					//terrain is always visible and affected by all shadows so must render
					projector->Peek_Material_Pass()->Install_Materials();
					DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices
					if (renderProjectedTerrainShadow(shadow, aaBox))
						projectionCount++;
					projector->Peek_Material_Pass()->UnInstall_Materials();

					SimpleObjectIterator *iter;
					Object *obj;

					iter = ThePartitionManager->iterateObjectsInRange((const Coord3D*)&sphere.Center,sphere.Radius, FROM_CENTER_3D);
					MemoryPoolObjectHolder hold( iter );

					AABoxIntersectionTestClass boxtest(aaBox,COLL_TYPE_ALL);

					for( obj = iter->first(); obj; obj = iter->next() )
					{
							Drawable *draw = obj->getDrawable();

							for (DrawModule ** dm = draw->getDrawModules(); *dm; ++dm)
							{
								const ObjectDrawInterface* di = (*dm)->getObjectDrawInterface();
								if (di)
								{
									W3DModelDraw *w3dDraw= (W3DModelDraw *)di;
									RenderObjClass *robj=nullptr;

									///@todo: don't apply shadows to translcuent objects unless they are MOBILE - hack to get tanks to work.
									if ((robj=w3dDraw->getRenderObject()) != nullptr && (!robj->Is_Alpha() || !obj->isKindOf(KINDOF_IMMOBILE)) && robj != shadow->m_robj && robj->Is_Really_Visible())
									{
											//do a more accurate test against W3D render bounding boxes.
											if (robj->Intersect_AABox(boxtest))
											{
												//Shadow reached a visible object so it needs to be rendered with shadow applied.
												rinfo.Push_Material_Pass(projector->Peek_Material_Pass());
												rinfo.Push_Override_Flags(RenderInfoClass::RINFO_OVERRIDE_ADDITIONAL_PASSES_ONLY);
												robj->Render(rinfo);	//WW3D::Render(*robj,rinfo);
												rinfo.Pop_Override_Flags();
												rinfo.Pop_Material_Pass();
												projectionCount++;	//keep track of number of shadow projections
											}
									}
								}
							}
					}
				}
			}
		}

		flushDecals(lastShadowDecalTexture,lastShadowType);	//make sure there are not any unrendered decals left over.
		TheDX8MeshRenderer.Flush();	//draw all the shadow receiving objects
	}
	if (m_decalList)
	{
		//keep track of active decal texture so we can render all decals at once.
		W3DShadowTexture *lastShadowDecalTexture=nullptr;
		ShadowType lastShadowType = SHADOW_NONE;

		for( shadow = m_decalList; shadow; shadow = shadow->m_next )
		{
			if (shadow->m_isEnabled && !shadow->m_isInvisibleEnabled)
			{
				if (lastShadowDecalTexture == nullptr)
					lastShadowDecalTexture=shadow->m_shadowTexture[0];
				if (lastShadowType == SHADOW_NONE)
					lastShadowType = shadow->m_type;

				if (shadow->m_shadowTexture[0] != lastShadowDecalTexture ||
					shadow->m_type != lastShadowType)
				{	flushDecals(lastShadowDecalTexture,lastShadowType);	//switched to a new texture, need to render polys using last texture.
					lastShadowDecalTexture=shadow->m_shadowTexture[0];
					lastShadowType=shadow->m_type;
				}
				///@todo: may need to fix this if shadows are large enough to be seen while object is not visible
				if (!(shadow->m_robj && !shadow->m_robj->Is_Really_Visible()))
				{	//queueSimpleDecal(shadow);
					queueDecal(shadow);	//only draw shadow if casting object is visible
					projectionCount++;
				}
			}
		}

		flushDecals(lastShadowDecalTexture,lastShadowType);	//make sure there are not any unrendered decals left over.
	}
	return projectionCount;
}

/** Generic function which can be used to create arbitrary decals that don't have to be used for shadows.
Some examples: Scorch marks, blood, stains, selection/status indicators, etc.*/
Shadow* W3DProjectedShadowManager::addDecal(Shadow::ShadowTypeInfo *shadowInfo)
{
	W3DShadowTexture *st=nullptr;
	ShadowType shadowType=SHADOW_NONE;		/// type of projection
	Bool	allowWorldAlign=FALSE;	/// wrap shadow around world geometry - else align perpendicular to local z-axis.
	Real	decalSizeX=0.0f;
	Real	decalSizeY=0.0f;

	Bool	allowSunDirection=FALSE;
	Char texture_name[ARRAY_SIZE(shadowInfo->m_ShadowName)];

	if (!shadowInfo)
		return nullptr;	//right now we require hardware render-to-texture support

	//simple decal using the premade texture specified.
	//can be always perpendicular to model's z-axis or projected
	//onto world geometry.
	strlcpy(texture_name, shadowInfo->m_ShadowName, ARRAY_SIZE(texture_name));
	strlcat(texture_name, ".tga", ARRAY_SIZE(texture_name));

	//Check if we previously added a decal using this texture
	st=m_W3DShadowTextureManager->getTexture(texture_name);
	if (st == nullptr)
	{
		//Adding a new decal texture
		TextureClass *w3dTexture=WW3DAssetManager::Get_Instance()->Get_Texture(texture_name);
		w3dTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		w3dTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		w3dTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);

		DEBUG_ASSERTCRASH(w3dTexture != nullptr, ("Could not load decal texture: %s",texture_name));

		if (!w3dTexture)
			return nullptr;

		st = NEW W3DShadowTexture;	// poolify
		SET_REF_OWNER( st );
		st->Set_Name(texture_name);
		m_W3DShadowTextureManager->addTexture( st );
		st->setTexture(w3dTexture);
	}

	shadowType=shadowInfo->m_type;
	allowWorldAlign=shadowInfo->allowWorldAlign;
	allowSunDirection=shadowInfo->m_type & SHADOW_DIRECTIONAL_PROJECTION;
	decalSizeX=shadowInfo->m_sizeX;
	decalSizeY=shadowInfo->m_sizeY;

	W3DProjectedShadow *shadow = NEW W3DProjectedShadow;	// poolify

	// sanity
	if( shadow == nullptr )
		return nullptr;

	shadow->setRenderObject(nullptr);
	shadow->setTexture(0,st);	///@todo: Fix projected shadows to allow multiple lights
	shadow->m_type = shadowType;		/// type of projection
	shadow->m_allowWorldAlign=allowWorldAlign;	/// wrap shadow around world geometry - else align perpendicular to local z-axis.

	shadow->m_oowDecalSizeX = 1.0f/decalSizeX;	//one over width
	shadow->m_oowDecalSizeY = 1.0f/decalSizeY;	//one over height

	//Prestore some values used during projection to optimize out division.
	shadow->m_decalSizeX = decalSizeX;	//width
	shadow->m_decalSizeY = decalSizeY;	//height

	shadow->m_decalOffsetU=0;
	shadow->m_decalOffsetV=0;

	shadow->m_flags	= allowSunDirection;

	shadow->init();

	// add to our shadow list through the shadow next links, insert next to other shadows using same texture

	W3DProjectedShadow *nextShadow=nullptr,*prevShadow=nullptr;
	for( nextShadow = m_decalList; nextShadow; prevShadow=nextShadow,nextShadow = nextShadow->m_next )
	{
		if (nextShadow->m_shadowTexture[0]==st)
		{	//found start of other shadows using same texture, insert new shadow here.
			shadow->m_next=nextShadow;
			if (prevShadow)
			{	prevShadow->m_next=shadow;
			}
			else
				m_decalList=shadow;
			break;
		}
	}

	if (nextShadow==nullptr)
	{	//shadow with new texture. Add to top of list.
		shadow->m_next = m_decalList;
		m_decalList = shadow;
	}

	updateShadowNumbers(shadow->m_type, +1);
	return shadow;
}

/** Generic function which can be used to create arbitrary decals that follow the renderObject but don't have to be used for shadows.
Some examples: Scorch marks, blood, stains, selection/status indicators, etc.*/
Shadow* W3DProjectedShadowManager::addDecal(RenderObjClass *robj, Shadow::ShadowTypeInfo *shadowInfo)
{
	W3DShadowTexture *st=nullptr;
	ShadowType shadowType=SHADOW_NONE;		/// type of projection
	Bool	allowWorldAlign=FALSE;	/// wrap shadow around world geometry - else align perpendicular to local z-axis.
	Real	decalSizeX=0.0f;
	Real	decalSizeY=0.0f;
	Real	decalOffsetX=0.0f;
	Real	decalOffsetY=0.0f;

	Bool	allowSunDirection=FALSE;
	Char texture_name[ARRAY_SIZE(shadowInfo->m_ShadowName)];

	if (!robj || !shadowInfo)
		return nullptr;	//right now we require hardware render-to-texture support

	//simple decal using the premade texture specified.
	//can be always perpendicular to model's z-axis or projected
	//onto world geometry.
	strlcpy(texture_name, shadowInfo->m_ShadowName, ARRAY_SIZE(texture_name));
	strlcat(texture_name, ".tga", ARRAY_SIZE(texture_name));

	//Check if we previously added a decal using this texture
	st=m_W3DShadowTextureManager->getTexture(texture_name);
	if (st == nullptr)
	{
		//Adding a new decal texture
		TextureClass *w3dTexture=WW3DAssetManager::Get_Instance()->Get_Texture(texture_name);
		w3dTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		w3dTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		w3dTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);

		DEBUG_ASSERTCRASH(w3dTexture != nullptr, ("Could not load decal texture: %s",texture_name));

		if (!w3dTexture)
			return nullptr;

		st = NEW W3DShadowTexture;
		SET_REF_OWNER( st );
		st->Set_Name(texture_name);
		m_W3DShadowTextureManager->addTexture( st );
		st->setTexture(w3dTexture);
	}

	shadowType=shadowInfo->m_type;
	allowWorldAlign=shadowInfo->allowWorldAlign;
	allowSunDirection=shadowInfo->m_type & SHADOW_DIRECTIONAL_PROJECTION;
	decalSizeX=shadowInfo->m_sizeX;
	decalSizeY=shadowInfo->m_sizeY;
	decalOffsetX=shadowInfo->m_offsetX;
	decalOffsetY=shadowInfo->m_offsetY;

	W3DProjectedShadow *shadow = NEW W3DProjectedShadow;

	// sanity
	if( shadow == nullptr )
		return nullptr;

	shadow->setRenderObject(robj);
	shadow->setTexture(0,st);	///@todo: Fix projected shadows to allow multiple lights
	shadow->m_type = shadowType;		/// type of projection
	shadow->m_allowWorldAlign=allowWorldAlign;	/// wrap shadow around world geometry - else align perpendicular to local z-axis.

	AABoxClass box;

	robj->Get_Obj_Space_Bounding_Box(box);

	//Check if app is overriding any of the default texture stretch factors.
	if (!decalSizeX)
		decalSizeX=box.Extent.X*2.0f;//use bounding box to determine size
	shadow->m_oowDecalSizeX = 1.0f/decalSizeX;	//one over width

	if (!decalSizeY)
		decalSizeY=box.Extent.Y*2.0f;//world space distance to stretch full texture
	shadow->m_oowDecalSizeY = 1.0f/decalSizeY;	//one over height

	if (decalOffsetX)
		decalOffsetX=decalOffsetX*shadow->m_oowDecalSizeX;

	if (decalOffsetY)
		decalOffsetY=decalOffsetY*shadow->m_oowDecalSizeY;

	//Prestore some values used during projection to optimize out division.
	shadow->m_decalSizeX = decalSizeX;	//width
	shadow->m_decalSizeY = decalSizeY;	//height

	shadow->m_decalOffsetU= decalOffsetX;
	shadow->m_decalOffsetV= decalOffsetY;

	shadow->m_flags	= allowSunDirection;
	if ((shadowType & SHADOW_DYNAMIC_PROJECTION) || (shadowInfo && shadowInfo->allowUpdates))
	{
		// GeneralsX @bugfix Copilot 11/05/2026 Keep projected shadow texture in sync for animated casters without root translation.
		shadow->m_flags |= SHADOW_DYNAMIC_PROJECTION;
	}

	shadow->init();

	// add to our shadow list through the shadow next links, insert next to other shadows using same texture

	W3DProjectedShadow *nextShadow=nullptr,*prevShadow=nullptr;
	for( nextShadow = m_decalList; nextShadow; prevShadow=nextShadow,nextShadow = nextShadow->m_next )
	{
		if (nextShadow->m_shadowTexture[0]==st)
		{	//found start of other shadows using same texture, insert new shadow here.
			shadow->m_next=nextShadow;
			if (prevShadow)
			{	prevShadow->m_next=shadow;
			}
			else
				m_decalList=shadow;
			break;
		}
	}

	if (nextShadow==nullptr)
	{	//shadow with new texture. Add to top of list.
		shadow->m_next = m_decalList;
		m_decalList = shadow;
	}

	updateShadowNumbers(shadow->m_type, +1);
	return shadow;
}

W3DProjectedShadow* W3DProjectedShadowManager::addShadow(RenderObjClass *robj, Shadow::ShadowTypeInfo *shadowInfo, Drawable *draw)
{
	W3DShadowTexture *st=nullptr;
	static char	defaultDecalName[]={"shadow.tga"};
	ShadowType shadowType=SHADOW_NONE;		/// type of projection
	Bool	allowWorldAlign=FALSE;	/// wrap shadow around world geometry - else align perpendicular to local z-axis.
	Real	decalSizeX=0.0f;
	Real	decalSizeY=0.0f;
	Real	decalOffsetX=0.0f;
	Real	decalOffsetY=0.0f;

	Bool	allowSunDirection=FALSE;
	Char texture_name[ARRAY_SIZE(shadowInfo->m_ShadowName)];


	if (!m_dynamicRenderTarget || !robj || !TheGlobalData->m_useShadowDecals)
		return nullptr;	//right now we require hardware render-to-texture support


	if (shadowInfo)
	{
		//determine what kind of shadow is needed
		// GeneralsX @bugfix Mr. Meeseeks 17/06/2026 Accept combined decal/alpha/additive flags via bitmask checks.
		if (shadowInfo->m_type & (SHADOW_DECAL | SHADOW_ALPHA_DECAL | SHADOW_ADDITIVE_DECAL))
		{		//simple decal using the premade texture specified.
				//can be always perpendicular to model's z-axis or projected
				//onto world geometry.
				if (strlen(shadowInfo->m_ShadowName) <= 1)	//no texture name given, use same as object
				{
					static_assert(ARRAY_SIZE(texture_name) >= ARRAY_SIZE(defaultDecalName), "Incorrect array size");
					strcpy(texture_name, defaultDecalName);
				}
				else
				{
					strlcpy(texture_name, shadowInfo->m_ShadowName, ARRAY_SIZE(texture_name));
					strlcat(texture_name, ".tga", ARRAY_SIZE(texture_name));
				}

				st=m_W3DShadowTextureManager->getTexture(texture_name);
				if (st == nullptr)
				{
					//need to add this texture without creating it from a real renderobject
					TextureClass *w3dTexture=WW3DAssetManager::Get_Instance()->Get_Texture(texture_name);
					w3dTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
					w3dTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
					w3dTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);

					DEBUG_ASSERTCRASH(w3dTexture != nullptr, ("Could not load decal texture"));

					if (!w3dTexture)
						return nullptr;

					st = NEW W3DShadowTexture;	// poolify
					SET_REF_OWNER( st );
					st->Set_Name(texture_name);
					m_W3DShadowTextureManager->addTexture( st );
					st->setTexture(w3dTexture);
				}
				shadowType=(ShadowType)(shadowInfo->m_type | SHADOW_DECAL);
				allowSunDirection=shadowInfo->m_type & SHADOW_DIRECTIONAL_PROJECTION;
				decalSizeX=shadowInfo->m_sizeX;
				decalSizeY=shadowInfo->m_sizeY;
				decalOffsetX=shadowInfo->m_offsetX;
				decalOffsetY=shadowInfo->m_offsetY;
		}
		else
		if (shadowInfo->m_type & (SHADOW_PROJECTION | SHADOW_DYNAMIC_PROJECTION))
		{		//projection of object geometry into a texture.
				//can be applied on a plane horizontal to model's z-axis or
				//projected onto world geometry.
				if (shadowInfo->m_ShadowName[0] != '\0')
				{	//the shadow will be based on another render object
					//to allow multiple models to share same shadow - for
					//example, all trees could use same shadow even if slightly
					//different color, etc.
					static_assert(ARRAY_SIZE(texture_name) >= ARRAY_SIZE(shadowInfo->m_ShadowName), "Incorrect array size");
					strcpy(texture_name, shadowInfo->m_ShadowName);
				}
				else
					strlcpy(texture_name, robj->Get_Name(), ARRAY_SIZE(texture_name));	//not texture name give, assume model name.

				st=m_W3DShadowTextureManager->getTexture(texture_name);
				if (st == nullptr)
				{	//texture doesn't exist, use current render object to create it
					m_W3DShadowTextureManager->createTexture(robj,texture_name);
					//try loading again
					st=m_W3DShadowTextureManager->getTexture(texture_name);

					DEBUG_ASSERTCRASH(st != nullptr, ("Could not create shadow texture"));

					if (st==nullptr)
						return nullptr;	//could not create the shadow texture
				}
				shadowType=(ShadowType)(shadowInfo->m_type | SHADOW_PROJECTION);
		}
	}
	else
	{	//no shadow info, assume user wants a projected shadow
		strlcpy(texture_name, robj->Get_Name(), ARRAY_SIZE(texture_name));

		st=m_W3DShadowTextureManager->getTexture(texture_name);

		if (st==nullptr)
		{	//did not find a cached copy of the shadow geometry, create a new one
			m_W3DShadowTextureManager->createTexture(robj,texture_name);
			//try loading again
			st=m_W3DShadowTextureManager->getTexture(texture_name);
			if (st==nullptr)
				return nullptr;	//could not create the shadow texture
		}
		shadowType=SHADOW_PROJECTION;
	}

	W3DProjectedShadow *shadow = NEW W3DProjectedShadow;

	// sanity
	if( shadow == nullptr )
		return nullptr;

	shadow->setRenderObject(robj);
	shadow->setTexture(0,st);	///@todo: Fix projected shadows to allow multiple lights
	shadow->m_type = shadowType;		/// type of projection
	shadow->m_allowWorldAlign=allowWorldAlign;	/// wrap shadow around world geometry - else align perpendicular to local z-axis.

	AABoxClass box;

	robj->Get_Obj_Space_Bounding_Box(box);

	//Check if app is overriding any of the default texture stretch factors.
	if (decalSizeX)
		decalSizeX=1.0f/decalSizeX; //world space distance to stretch full texture scale
	else
		decalSizeX=1.0f/(box.Extent.X*2.0f);//use bounding box to determine size

	if (decalSizeY)
		decalSizeY=-1.0f/decalSizeY;
	else
		decalSizeY=-1.0f/(box.Extent.Y*2.0f);//world space distance to stretch full texture

	if (decalOffsetX)
		decalOffsetX=-decalOffsetX*decalSizeX;
	else
		decalOffsetX=0.0f;//-box.Center.X*decalSizeX;

	if (decalOffsetY)
		decalOffsetY=-decalOffsetY*decalSizeY;
	else
		decalOffsetY=0.0f;//-box.Center.Y*decalSizeY;

	//Prestore some values used during projection to optimize out division.
	shadow->m_oowDecalSizeX = decalSizeX;	//one over width
	shadow->m_oowDecalSizeY = decalSizeY;	//one over height
	shadow->m_decalSizeX = 1.0f/decalSizeX;	//width
	shadow->m_decalSizeY = 1.0f/decalSizeY;	//height

	shadow->m_decalOffsetU= decalOffsetX;
	shadow->m_decalOffsetV= decalOffsetY;

	shadow->m_flags	= allowSunDirection;
	if ((shadowType & SHADOW_DYNAMIC_PROJECTION) || (shadowInfo && shadowInfo->allowUpdates))
	{
		// GeneralsX @bugfix Copilot 11/05/2026 Keep projected shadow texture in sync for animated casters without root translation.
		shadow->m_flags |= SHADOW_DYNAMIC_PROJECTION;
	}

	shadow->init();

	// add to our shadow list through the shadow next links, insert next to other shadows using same texture

	W3DProjectedShadow *nextShadow=nullptr,*prevShadow=nullptr;
	for( nextShadow = m_shadowList; nextShadow; prevShadow=nextShadow,nextShadow = nextShadow->m_next )
	{
		if (nextShadow->m_shadowTexture[0]==st)
		{	//found start of other shadows using same texture, insert new shadow here.
			shadow->m_next=nextShadow;
			if (prevShadow)
			{	prevShadow->m_next=shadow;
			}
			else
				m_shadowList=shadow;
			break;
		}
	}

	if (nextShadow==nullptr)
	{	//shadow with new texture. Add to top of list.
		shadow->m_next = m_shadowList;
		m_shadowList = shadow;
	}

	updateShadowNumbers(shadow->m_type, +1);
	return shadow;
}

W3DProjectedShadow* W3DProjectedShadowManager::createDecalShadow(Shadow::ShadowTypeInfo *shadowInfo)
{
	W3DShadowTexture *st=nullptr;
	static char	defaultDecalName[]={"shadow.tga"};
	ShadowType shadowType=SHADOW_DECAL;		/// type of projection
	Bool	allowWorldAlign=FALSE;	/// wrap shadow around world geometry - else align perpendicular to local z-axis.
	Real	decalSizeX=0.0f;
	Real	decalSizeY=0.0f;
	Real	decalOffsetX=0.0f;
	Real	decalOffsetY=0.0f;
	const Real defaultWidth = 10.0f;
	Char texture_name[ARRAY_SIZE(shadowInfo->m_ShadowName)];

	//simple decal using the premade texture specified.
	//can be always perpendicular to model's z-axis or projected
	//onto world geometry.
	if (strlen(shadowInfo->m_ShadowName) <= 1)	//no texture name given, use same as object
	{
		static_assert(ARRAY_SIZE(texture_name) >= ARRAY_SIZE(defaultDecalName), "Incorrect array size");
		strcpy(texture_name,defaultDecalName);
	}
	else
	{
		strlcpy(texture_name, shadowInfo->m_ShadowName, ARRAY_SIZE(texture_name));
		strlcat(texture_name, ".tga", ARRAY_SIZE(texture_name));
	}

	st=m_W3DShadowTextureManager->getTexture(texture_name);
	if (st == nullptr)
	{
		//need to add this texture without creating it from a real renderobject
		TextureClass *w3dTexture=WW3DAssetManager::Get_Instance()->Get_Texture(texture_name);
		w3dTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		w3dTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		w3dTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);

		DEBUG_ASSERTCRASH(w3dTexture != nullptr, ("Could not load decal texture"));

		if (!w3dTexture)
			return nullptr;

		st = NEW W3DShadowTexture;	// poolify
		SET_REF_OWNER( st );
		st->Set_Name(texture_name);
		m_W3DShadowTextureManager->addTexture( st );
		st->setTexture(w3dTexture);
	}
	shadowType=SHADOW_DECAL;
	decalSizeX=shadowInfo->m_sizeX;
	decalSizeY=shadowInfo->m_sizeY;
	decalOffsetX=shadowInfo->m_offsetX;
	decalOffsetY=shadowInfo->m_offsetY;

	W3DProjectedShadow *shadow = NEW W3DProjectedShadow;

	// sanity
	if( shadow == nullptr )
		return nullptr;

	shadow->setTexture(0,st);
	shadow->m_type = shadowType;		/// type of projection
	shadow->m_allowWorldAlign=allowWorldAlign;	/// wrap shadow around world geometry - else align perpendicular to local z-axis.


	//Check if app is overriding any of the default texture stretch factors.
	if (decalSizeX)
		decalSizeX=1.0f/decalSizeX; //world space distance to stretch full texture scale
	else
		decalSizeX=1.0f/(defaultWidth*2.0f);//use bounding box to determine size

	if (decalSizeY)
		decalSizeY=-1.0f/decalSizeY;
	else
		decalSizeY=-1.0f/(defaultWidth*2.0f);//world space distance to stretch full texture

	if (decalOffsetX)
		decalOffsetX=-decalOffsetX*decalSizeX;
	else
		decalOffsetX=0.0f;//-box.Center.X*decalSizeX;

	if (decalOffsetY)
		decalOffsetY=-decalOffsetY*decalSizeY;
	else
		decalOffsetY=0.0f;//-box.Center.Y*decalSizeY;

	//Prestore some values used during projection to optimize out division.
	shadow->m_oowDecalSizeX = decalSizeX;	//one over width
	shadow->m_oowDecalSizeY = decalSizeY;	//one over height
	shadow->m_decalSizeX = 1.0f/decalSizeX;	//width
	shadow->m_decalSizeY = 1.0f/decalSizeY;	//height

	shadow->m_decalOffsetU= decalOffsetX;
	shadow->m_decalOffsetV= decalOffsetY;

	shadow->m_flags	= 0;

	shadow->init();

	return shadow;
}

void W3DProjectedShadowManager::removeShadow (W3DProjectedShadow *shadow)
{
	W3DProjectedShadow *prev_shadow=nullptr;
	W3DProjectedShadow *next_shadow=nullptr;

	if (shadow->m_type & (SHADOW_ALPHA_DECAL|SHADOW_ADDITIVE_DECAL))
	{
		for( next_shadow = m_decalList; next_shadow; prev_shadow=next_shadow, next_shadow = next_shadow->m_next )
		{
			if (next_shadow == shadow)
			{
				if (prev_shadow)
					prev_shadow->m_next=shadow->m_next;
				else
					m_decalList=shadow->m_next;

				updateShadowNumbers(shadow->m_type, -1);
				delete shadow;
				return;
			}
		}
	}

	//search for this shadow
	for( next_shadow = m_shadowList; next_shadow; prev_shadow=next_shadow, next_shadow = next_shadow->m_next )
	{
		if (next_shadow == shadow)
		{
			if (prev_shadow)
				prev_shadow->m_next=shadow->m_next;
			else
				m_shadowList=shadow->m_next;

			updateShadowNumbers(shadow->m_type, -1);
			delete shadow;
			return;
		}
	}
}

void W3DProjectedShadowManager::removeAllShadows()
{

	W3DProjectedShadow *cur_shadow=nullptr;
	W3DProjectedShadow *next_shadow=m_shadowList;
	m_shadowList = nullptr;
	m_numDecalShadows  = 0;
	m_numProjectionShadows = 0;

	//search for this shadow
	for( cur_shadow = next_shadow; cur_shadow; cur_shadow = next_shadow )
	{
		next_shadow = cur_shadow->m_next;
		cur_shadow->m_next = nullptr;
		delete cur_shadow;
	}

	next_shadow=m_decalList;
	cur_shadow=nullptr;
	m_decalList=nullptr;
	for( cur_shadow = next_shadow; cur_shadow; cur_shadow = next_shadow )
	{
		next_shadow = cur_shadow->m_next;
		cur_shadow->m_next = nullptr;
		delete cur_shadow;
	}
}

void W3DProjectedShadowManager::updateShadowNumbers(ShadowType shadowType, Int addNum)
{
	// GeneralsX @bugfix Meeseeks 17/06/2026 Support combined shadow flags by checking bits.
	if (shadowType & (SHADOW_DECAL | SHADOW_ALPHA_DECAL | SHADOW_ADDITIVE_DECAL))
	{
		m_numDecalShadows += addNum;
	}
	else if (shadowType & SHADOW_PROJECTION)
	{
		m_numProjectionShadows += addNum;
	}
}

#if defined(RTS_DEBUG)
void W3DProjectedShadow::getRenderCost(RenderCost & rc) const
{
	if (TheGlobalData->m_useShadowDecals && m_isEnabled && !m_isInvisibleEnabled)
		rc.addShadowDrawCalls(1);
}
#endif

W3DProjectedShadow::W3DProjectedShadow()
{
	m_diffuse=0xffffffff;
	m_shadowProjector=nullptr;
	m_lastObjPosition.Set(0,0,0);
	m_type = SHADOW_NONE;		/// type of projection
	m_allowWorldAlign = FALSE;	/// wrap shadow around world geometry - else align perpendicular to local z-axis.
	m_isEnabled = TRUE;
	m_isInvisibleEnabled = FALSE;
	for (Int i=0; i<MAX_SHADOW_LIGHTS; i++)
		m_shadowTexture[i]=nullptr;
}

W3DProjectedShadow::~W3DProjectedShadow()
{
	REF_PTR_RELEASE(m_shadowProjector);
	for (Int i=0; i<MAX_SHADOW_LIGHTS; i++)
		REF_PTR_RELEASE(m_shadowTexture[i]);
}

void W3DProjectedShadow::init()
{

	DEBUG_ASSERTCRASH(m_shadowProjector == nullptr, ("Init of existing shadow projector"));

	if (m_type & SHADOW_PROJECTION)
	{
		float	minx,maxx;
		m_shadowProjector = NEW_REF(TexProjectClass,());
		m_shadowProjector->Set_Intensity(0.4f,true);
		m_shadowProjector->Set_Texture(m_shadowTexture[0]->getTexture());
	}
}

#define DECAL_TEXELS_PER_WORLD_UNIT	(64.0f/20.0f)	//64 texels per 2 terrain cells (20 units)

void W3DProjectedShadow::updateTexture(Vector3 &lightPos)
{
	SpecialRenderInfoClass *context;
	//default uv coordinates before rotation starting at top/left going clockwise
	static Vector2 uvData[4]={Vector2(-0.5,-0.5f),Vector2(-0.5,0.5f),Vector2(0.5f,0.5f),Vector2(-0.5f,0.5f)};

	//force light always 2000 units from object - for some reason projection fails if
	//light is too far.
	///@todo: See why infinite light sources don't project shadows correctly.

	// GeneralsX @bugfix Meeseeks 17/06/2026 Support combined projection flags by checking bits.
	if (m_type & SHADOW_PROJECTION)
	{	//projected shadows use custom runtime generated textures based on object geometry
		Vector3 objPos=m_robj->Get_Position();
		if (objPos == Vector3(0,0,0))
			return; //render object does not have a valid position (never rendered).
		Vector3 objToLight=lightPos - objPos;
		objToLight.Normalize();
		objToLight =  objPos + objToLight * 2000.0f;

		m_shadowProjector->Compute_Perspective_Projection(m_robj,objToLight);
		m_shadowProjector->Set_Render_Target(TheW3DProjectedShadowManager->getRenderTarget());

		//Set ambient to 0, so we get a black shadow on solid background

		context=TheW3DProjectedShadowManager->getRenderContext();

		context->light_environment->Reset(m_robj->Get_Position(), Vector3(0,0,0));

		m_shadowProjector->Compute_Texture(m_robj,context);

		//Need to copy generated texture into permanent texture.
		SurfaceClass *oldSurface=m_shadowTexture[0]->getTexture()->Get_Surface_Level();
		SurfaceClass *newSurface=TheW3DProjectedShadowManager->getRenderTarget()->Get_Surface_Level();

		//Copy shadow from temporary video-memory surface into a permanent texture
		oldSurface->Copy(0,0,0,0,DEFAULT_RENDER_TARGET_WIDTH,DEFAULT_RENDER_TARGET_HEIGHT,newSurface);
		REF_PTR_RELEASE(newSurface);
		REF_PTR_RELEASE(oldSurface);
		m_shadowTexture[0]->updateBounds(TheW3DShadowManager->getLightPosWorld(0),m_robj);	//update local shadow bounds
	}
	else
	if (m_type & SHADOW_DECAL)
	{	//decal shadows use artist supplied textures.  We just need to tweak the uv coordinates to match
		//the light direction.
		Vector3 objPos=m_robj->Get_Position();
		Vector3 objectToLight;
		if (m_flags & SHADOW_DIRECTIONAL_PROJECTION)
		{	objectToLight=lightPos-objPos;
			//we're ignoring sun's distance from horizon, so drop vertical component
			objectToLight.Z=0;
			objectToLight.Normalize();
		}
		else
			objectToLight.Set(1.0f,0.0f,0.0f);

		SurfaceClass::SurfaceDescription surface_desc;
		m_shadowTexture[0]->getTexture()->Get_Level_Description(surface_desc);
		//default shadow texture points along world -x axis (west).  Rotate uv coordinates to fit actual light direction
		Vector3 uVec = objectToLight * DECAL_TEXELS_PER_WORLD_UNIT / (float)surface_desc.Width;
		objectToLight.Rotate_Z(-1.0f,0.0f);	//rotate u vector by -90 degress to get v vector.
		Vector3 vVec = objectToLight * DECAL_TEXELS_PER_WORLD_UNIT / (float)surface_desc.Height;

		m_shadowTexture[0]->setDecalUVAxis(uVec, vVec);

		///@todo: tweak decal bounding volumes to something sensible
		AABoxClass	box;
		SphereClass	sphere;
		m_robj->Get_Obj_Space_Bounding_Box(box);	//shadow uses same bounding box as object
		m_robj->Get_Obj_Space_Bounding_Sphere(sphere);

		m_shadowTexture[0]->setBoundingSphere(sphere);
		m_shadowTexture[0]->setBoundingBox(box);
	}

	m_shadowTexture[0]->setLightPosHistory(lightPos);	//store position of light at time of texture update.

}


void W3DProjectedShadow::updateProjectionParameters(const Matrix3D &cameraXform)
{
		m_shadowProjector->Pre_Render_Update(cameraXform);
}

void W3DProjectedShadow::update()
{
	if (m_shadowTexture[0]->getLightPosHistory() != TheW3DShadowManager->getLightPosWorld(0))
	{	//light has moved since last time this shadow was calculated. Need update
		updateTexture(TheW3DShadowManager->getLightPosWorld(0));
	}
	else if (m_flags & SHADOW_DYNAMIC_PROJECTION)
	{
		// GeneralsX @bugfix Copilot 11/05/2026 Animated sub-mesh shadows need per-frame texture refresh even without object translation.
		updateTexture(TheW3DShadowManager->getLightPosWorld(0));
	}
	if (m_lastObjPosition != m_robj->Get_Position())
	{	//object has moved.  Texture stays the same but projection matrix needs updating.
		//force light always 2000 units from object - for some reason projection fails if
		//light is too far.
		///@todo: See why infinite light sources don't project shadows correctly.
		if (m_type & SHADOW_PROJECTION)
		{
			Vector3 objToLight=TheW3DShadowManager->getLightPosWorld(0) - m_robj->Get_Position();
			objToLight.Normalize();
			objToLight =  m_robj->Get_Position() + objToLight * 2000.0f;

			m_shadowProjector->Compute_Perspective_Projection(m_robj,objToLight);
		}
		setObjPosHistory(m_robj->Get_Position());
	}
}

Int W3DShadowTexture::init(RenderObjClass *robj)
{
	///@todo: implement this function
	SurfaceClass::SurfaceDescription surface_desc;

	TheW3DProjectedShadowManager->getRenderTarget()->Get_Level_Description(surface_desc);

	TextureClass *new_texture = MSGNEW("TextureClass") TextureClass(surface_desc.Width,surface_desc.Height,surface_desc.Format,MIP_LEVELS_1);

	setTexture(new_texture);

	return TRUE;
}

void W3DShadowTexture::updateBounds(Vector3 &lightPos, RenderObjClass *robj)
{
		AABoxClass	&box=m_areaEffectBox;	///@todo: fix for multiple lights
		Vector3			objPos;
		Vector3 Corners[8];
		Vector3 lightRay;
		Real floorZ;
		Real vectorScale,vectorScaleTemp, vectorScaleMax,length;

		//calculate local bounding box of shadow projection
		objPos=robj->Get_Position();
		box=robj->Get_Bounding_Box();
		floorZ = objPos.Z - 2.0f;	//lower slightly so shadows go under ground.

		//project each box corner to base of object to determine rough extent of shadow
		//Get vertices of top of bounding box
		Corners[0]=box.Center+box.Extent;	//top right corner
		Corners[1]=Corners[0];
		Corners[1].X -= 2.0f*box.Extent.X;		//top left corner
		Corners[2]=Corners[1];
		Corners[2].Y -= 2.0f*box.Extent.Y;		//bottom left corner
		Corners[3]=Corners[2];
		Corners[3].X += 2.0f*box.Extent.X;		//bottom right corner

		//Project top volume corners onto ground plane
		lightRay = Corners[0] - lightPos;	//vector light to corner
		length= 1.0f/lightRay.Length();
		lightRay *= length;
		vectorScaleMax=vectorScale=(Real)fabs((Corners[0].Z-floorZ)/lightRay.Z);	//length of vector from top corner to ground.
		Corners[4]=Corners[0]+lightRay*vectorScale;
		vectorScaleMax *= length;

		lightRay = Corners[1] - lightPos;	//vector light to corner
		length= 1.0f/lightRay.Length();
		lightRay *= length;
		vectorScaleTemp=(Real)fabs((Corners[1].Z-floorZ)/lightRay.Z);	//length of vector from top corner to ground.
		Corners[5]=Corners[1]+lightRay*vectorScaleTemp;
		vectorScaleTemp *= length;

		if (vectorScaleTemp > vectorScaleMax)
			vectorScaleMax=vectorScaleTemp;	//keep track of maximum required extrusion length.

		lightRay = Corners[2] - lightPos;	//vector light to corner
		length= 1.0f/lightRay.Length();
		lightRay *= length;
		vectorScale=(Real)fabs((Corners[2].Z-floorZ)/lightRay.Z);	//length of vector from top corner to ground.
		Corners[6]=Corners[2]+lightRay*vectorScale;
		vectorScale *= length;

		if (vectorScale > vectorScaleMax)
			vectorScaleMax=vectorScale;	//keep track of maximum required extrusion length.

		lightRay = Corners[3] - lightPos;	//vector light to corner
		length= 1.0f/lightRay.Length();
		lightRay *= length;
		vectorScaleTemp=(Real)fabs((Corners[3].Z-floorZ)/lightRay.Z);	//length of vector from top corner to ground.
		Corners[7]=Corners[3]+lightRay*vectorScaleTemp;
		vectorScaleTemp *= length;

		if (vectorScaleTemp > vectorScaleMax)
			vectorScaleMax=vectorScaleTemp;	//keep track of maximum required extrusion length.

		box.Init(Corners, 8);	//generate a new bounding box to fit the shadow projection
		m_areaEffectSphere.Init(box.Center,box.Extent.Length());

		m_areaEffectSphere.Center -= objPos;	//translate sphere to object space.
		box.Translate(-objPos);	//translate box to object space.
}

W3DShadowTextureManager::W3DShadowTextureManager()
{
	// Create the hash tables
	texturePtrTable = NEW HashTableClass( 2048 );
	missingTextureTable = NEW HashTableClass( 2048 );
}

W3DShadowTextureManager::~W3DShadowTextureManager()
{
	freeAllTextures();

	delete texturePtrTable;
	texturePtrTable = nullptr;

	delete missingTextureTable;
	missingTextureTable = nullptr;
}

/** Release all loaded textures */
void W3DShadowTextureManager::freeAllTextures()
{
	// Make an iterator, and release all ptrs
	W3DShadowTextureManagerIterator it( *this );
	for( it.First(); !it.Is_Done(); it.Next() ) {
		W3DShadowTexture *text = it.getCurrentTexture();
		text->Release_Ref();
	}

	// Then clear the table
	texturePtrTable->Reset();
}

/** Find texture in cache */
W3DShadowTexture * W3DShadowTextureManager::peekTexture(const char * name)
{
	return (W3DShadowTexture*)texturePtrTable->Find( name );
}

/** Get texture from cache and increment its reference count */
W3DShadowTexture * W3DShadowTextureManager::getTexture(const char * name)
{
	W3DShadowTexture * text = peekTexture( name );
	if ( text != nullptr ) {
		text->Add_Ref();
	}
	return text;
}

/** Add texture to cache */
Bool W3DShadowTextureManager::addTexture(W3DShadowTexture *newTexture)
{
	WWASSERT (newTexture != nullptr);

	// Increment the refcount on the new texture and add it to our table.
	newTexture->Add_Ref ();
	texturePtrTable->Add( newTexture );

	return true;
}

void W3DShadowTextureManager::invalidateCachedLightPositions()
{
	// step through each of our shadow textures and update previous light position.
	Vector3 idVec(0,0,0);

	W3DShadowTextureManagerIterator it( *this );
	for( it.First(); !it.Is_Done(); it.Next() )
	{
		W3DShadowTexture *text = it.getCurrentTexture();
		text->setLightPosHistory(idVec);
	}
}

/*
** An entry for a table of textures not found, so we can quickly determine their loss
*/
class MissingTextureClass : public HashableClass {

public:
	MissingTextureClass( const char * name ) : Name( name ) {}
	virtual	~MissingTextureClass() override {}

	virtual	const char * Get_Key() override { return Name;	}

private:
	StringClass	Name;

};

/*
** Missing Textures
**
** The idea here, allow the system to register which textures are determined to be missing
** so that if they are asked for again, we can quickly return nullptr, without searching again.
*/
void	W3DShadowTextureManager::registerMissing( const char * name )
{
	missingTextureTable->Add( NEW MissingTextureClass( name ) );
}

Bool	W3DShadowTextureManager::isMissing( const char * name )
{
	return ( missingTextureTable->Find( name ) != nullptr );
}

/** Create shadow geometry from a reference W3D RenderObject*/
int W3DShadowTextureManager::createTexture(RenderObjClass *robj, const char *name)
{
	Bool res=FALSE;

	W3DShadowTexture * newTexture = NEW W3DShadowTexture;

	if (newTexture == nullptr) {
		goto Error;
	}

	SET_REF_OWNER( newTexture );
	newTexture->Set_Name(name);

	res=newTexture->init(robj);

	if (res != TRUE)
	{	// load failed!
		newTexture->Release_Ref();
		goto Error;
	} else if (peekTexture(newTexture->Get_Name()) != nullptr)
	{	// duplicate exists!
		newTexture->Release_Ref();	// Release the one we just loaded
		goto Error;
	} else
	{	addTexture( newTexture );
		newTexture->Release_Ref();
	}

	return 0;

Error:
	return 1;
}
