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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /VSS_Sync/ww3d2/meshmdl.cpp                                 $*
 *                                                                                             *
 *                       Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                     $Modtime:: 8/29/01 7:29p                                               $*
 *                                                                                             *
 *                    $Revision:: 46                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "meshmdl.h"
#include "matinfo.h"
#include "aabtree.h"
#include "htree.h"
#include "vp.h"
#include "visrasterizer.h"
#include "dx8polygonrenderer.h"
#include "bwrender.h"
#include "camera.h"
#include "dx8renderer.h"
#include "hashtemplate.h"


/*
** Temporary Buffers
** These buffers are used by the skin code for temporary storage of the deformed vertices and
** vertex normals.
*/
static DynamicVectorClass<Vector3>	_TempVertexBuffer;
static DynamicVectorClass<Vector3>	_TempNormalBuffer;
static DynamicVectorClass<Vector4>	_TempTransformedVertexBuffer;
static DynamicVectorClass<unsigned long> _TempClipFlagBuffer;


/*
**
** MeshModelClass Implementation
**
*/


MeshModelClass::MeshModelClass() :
	DefMatDesc(nullptr),
	AlternateMatDesc(nullptr),
	CurMatDesc(nullptr),
	MatInfo(nullptr),
	GapFiller(nullptr)
{
	Set_Flag(DIRTY_BOUNDS,true);

	DefMatDesc = W3DNEW MeshMatDescClass;
	CurMatDesc = DefMatDesc;

	MatInfo = NEW_REF( MaterialInfoClass, () );
}

MeshModelClass::MeshModelClass(const MeshModelClass & that) :
	MeshGeometryClass(that),
	DefMatDesc(nullptr),
	AlternateMatDesc(nullptr),
	CurMatDesc(nullptr),
	MatInfo(nullptr),
	GapFiller(nullptr)
{
	DefMatDesc = W3DNEW MeshMatDescClass(*(that.DefMatDesc));
	if (that.AlternateMatDesc != nullptr) {
		AlternateMatDesc = W3DNEW MeshMatDescClass(*(that.AlternateMatDesc));
	}
	CurMatDesc = DefMatDesc;

	clone_materials(that);
}

MeshModelClass::~MeshModelClass()
{

	Reset(0,0,0);
	REF_PTR_RELEASE(MatInfo);

	delete DefMatDesc;
	delete AlternateMatDesc;

}

MeshModelClass & MeshModelClass::operator = (const MeshModelClass & that)
{
	if (this != &that) {
		// Remove all polygon renderers, this will remove the mesh from the rendering system.
		// The mesh will be initialized to rendering system the next time it is rendered.
		TheDX8MeshRenderer.Unregister_Mesh_Type(this);

		MeshGeometryClass::operator = (that);

		*DefMatDesc = *(that.DefMatDesc);
		CurMatDesc = DefMatDesc;

		delete AlternateMatDesc;
		AlternateMatDesc = nullptr;

		if (that.AlternateMatDesc != nullptr) {
			AlternateMatDesc = W3DNEW MeshMatDescClass(*(that.AlternateMatDesc));
		}

		clone_materials(that);

		delete GapFiller;
			GapFiller=nullptr;

		if (that.GapFiller)
			GapFiller=W3DNEW GapFillerClass(*that.GapFiller);
	}
	return * this;
}

void MeshModelClass::Reset(int polycount,int vertcount,int passcount)
{
	//DMS - We must delete the gapfiller object BEFORE the geometry is reset.  Otherwise,
	// the number of stages and passes gets reset and the gapfiller cannot deallocate properly.
	delete GapFiller;
	GapFiller=nullptr;

	Reset_Geometry(polycount,vertcount);

	// Release everything we have and reset to initial state

	TheDX8MeshRenderer.Unregister_Mesh_Type(this);

	MatInfo->Reset();
	DefMatDesc->Reset(polycount,vertcount,passcount);

	delete AlternateMatDesc;
	AlternateMatDesc = nullptr;

	CurMatDesc = DefMatDesc;
}

void MeshModelClass::Register_For_Rendering()
{
//WW3D::Set_NPatches_Level(1);
	if (WW3D::Get_NPatches_Level()>1) {
		if (WW3D::Get_NPatches_Gap_Filling_Mode()!=WW3D::NPATCHES_GAP_FILLING_DISABLED) {
			Init_For_NPatch_Rendering();
		}
		else {
			delete GapFiller;
			GapFiller=nullptr;
		}
	}
	else {
		if (WW3D::Get_NPatches_Gap_Filling_Mode()==WW3D::NPATCHES_GAP_FILLING_FORCE) {
			Init_For_NPatch_Rendering();
		}
		else {
			delete GapFiller;
			GapFiller=nullptr;
		}
	}

	TheDX8MeshRenderer.Register_Mesh_Type(this);
}

void MeshModelClass::Replace_Texture(TextureClass* texture,TextureClass* new_texture)
{
	WWASSERT(texture);
	WWASSERT(new_texture);
	for (int stage=0;stage<MeshMatDescClass::MAX_TEX_STAGES;++stage) {
		for (int pass=0;pass<Get_Pass_Count();++pass) {
			if (Has_Texture_Array(pass,stage)) {
				for (int i=0;i<Get_Polygon_Count();++i) {
					if (Peek_Texture(i,pass,stage)==texture) {
						Set_Texture(i,new_texture,pass,stage);
					}
				}
			}
			else {
				if (Peek_Single_Texture(pass,stage)==texture) {
					Set_Single_Texture(new_texture,pass,stage);
				}
			}
			// If this mesh model has been initialized for rendering we need to tell the rendering
			// system to change texturing as well.
			DX8FVFCategoryContainer* fvf_category=Peek_FVF_Category_Container();
			if (fvf_category) {
				fvf_category->Change_Polygon_Renderer_Texture(PolygonRendererList,texture,new_texture,pass,stage);
			}
		}
	}
}

void MeshModelClass::Replace_VertexMaterial(VertexMaterialClass* vmat,VertexMaterialClass* new_vmat)
{
	WWASSERT(vmat);
	WWASSERT(new_vmat);

	for (int pass=0;pass<Get_Pass_Count();++pass) {
		if (Has_Material_Array(pass)) {
			for (int i=0;i<Get_Vertex_Count();++i) {
				if (Peek_Material(i,pass)==vmat) {
					Set_Material(i,new_vmat,pass);
				}
			}
		}
		else {
			if (Peek_Single_Material(pass)==vmat) {
				Set_Single_Material(new_vmat,pass);
			}
		}
		// If this mesh model has been initialized for rendering we need to tell the rendering
		// system to change texturing as well.
		DX8FVFCategoryContainer* fvf_category=Peek_FVF_Category_Container();
		if (fvf_category) {
			fvf_category->Change_Polygon_Renderer_Material(PolygonRendererList,vmat,new_vmat,pass);
		}
	}
}

DX8FVFCategoryContainer* MeshModelClass::Peek_FVF_Category_Container()
{
	if (PolygonRendererList.Is_Empty()) return nullptr;
	DX8PolygonRendererClass* polygon_renderer=PolygonRendererList.Get_Head();
	WWASSERT(polygon_renderer);
	DX8TextureCategoryClass* texture_category=polygon_renderer->Get_Texture_Category();
	WWASSERT(texture_category);
	DX8FVFCategoryContainer* fvf_category=texture_category->Get_Container();
	WWASSERT(fvf_category);
	return fvf_category;
}

void MeshModelClass::Shadow_Render(SpecialRenderInfoClass & rinfo,const Matrix3D & tm,const HTreeClass * htree)
{
	if (rinfo.BWRenderer != nullptr) {
		if (_TempTransformedVertexBuffer.Length() < VertexCount) _TempTransformedVertexBuffer.Resize(VertexCount);
		Vector4* transf_ptr=&(_TempTransformedVertexBuffer[0]);
		get_deformed_screenspace_vertices(transf_ptr,rinfo,tm,htree);

		Vector2* tptr = reinterpret_cast<Vector2 *>(transf_ptr);
		Vector4* optr = transf_ptr;
		for (int a=0;a<VertexCount;++a,++optr) *tptr++=Vector2((*optr)[0],-(*optr)[1]);

		rinfo.BWRenderer->Set_Vertex_Locations(reinterpret_cast<Vector2*>(transf_ptr),VertexCount);
		rinfo.BWRenderer->Render_Triangles(reinterpret_cast<const unsigned long*>(Poly->Get_Array()),PolyCount*3);
		return;
	}
}

// Destination pointers MUST point to arrays large enough to hold all vertices
void MeshModelClass::get_deformed_vertices(Vector3 *dst_vert,const HTreeClass * htree)
{
	Vector3 * src_vert = Vertex->Get_Array();
	uint16 * bonelink = VertexBoneLink->Get_Array();
	for (int vi = 0; vi < Get_Vertex_Count(); vi++) {
		const Matrix3D & tm = htree->Get_Transform(bonelink[vi]);
		Matrix3D::Transform_Vector(tm, src_vert[vi], &(dst_vert[vi]));
	}
}


// Destination pointers MUST point to arrays large enough to hold all vertices
void MeshModelClass::get_deformed_vertices(Vector3 *dst_vert, Vector3 *dst_norm,const HTreeClass * htree)
{
	int vi;
	int vertex_count=Get_Vertex_Count();
	Vector3 * src_vert = Vertex->Get_Array();
#if (OPTIMIZE_VNORMS)
	Vector3 * src_norm = (Vector3 *)Get_Vertex_Normal_Array();
#else
	Vector3 * src_norm = VertexNorm->Get_Array();
#endif
	uint16 * bonelink = VertexBoneLink->Get_Array();

	for (vi = 0; vi < vertex_count;) {
		const Matrix3D & tm = htree->Get_Transform(bonelink[vi]);

		// Make a copy so we can set the translation to zero
		Matrix3D mytm=tm;

		int idx=bonelink[vi];
		int cnt;
		for (cnt = vi; cnt < vertex_count; cnt++) {
			if (idx!=bonelink[cnt]) {
				break;
			}
		}

		VectorProcessorClass::Transform(dst_vert+vi,src_vert+vi,mytm,cnt-vi);
		mytm.Set_Translation(Vector3(0.0f,0.0f,0.0f));
		VectorProcessorClass::Transform(dst_norm+vi,src_norm+vi,mytm,cnt-vi);
		vi=cnt;
	}
}

// Destination pointer MUST point to arrays large enough to hold all vertices
void MeshModelClass::compose_deformed_vertex_buffer(
	VertexFormatXYZNDUV2* verts,
	const Vector2* uv0,
	const Vector2* uv1,
	const unsigned* diffuse,
	const HTreeClass * htree)
{
	int vi;
	int vertex_count=Get_Vertex_Count();
	Vector3 * src_vert = Vertex->Get_Array();
#if (OPTIMIZE_VNORMS)
	Vector3 * src_norm = (Vector3 *)Get_Vertex_Normal_Array();
#else
	Vector3 * src_norm = VertexNorm->Get_Array();
#endif
	uint16 * bonelink = VertexBoneLink->Get_Array();

	for (vi = 0; vi < vertex_count;) {
		const Matrix3D & tm = htree->Get_Transform(bonelink[vi]);

		// Make a copy so we can set the translation to zero
		Matrix3D mytm=tm;

		int idx=bonelink[vi];
		int cnt;
		for (cnt = vi; cnt < vertex_count; cnt++) {
			if (idx!=bonelink[cnt]) {
				break;
			}
		}

		for (int pidx=0;pidx<cnt-vi;++pidx) {
			const Matrix3D& A=mytm;
			VertexFormatXYZNDUV2* out=verts+vi+pidx;
			const Vector3& v=*(src_vert+vi+pidx);
			out->x = (A[0][0] * v.X + A[0][1] * v.Y + A[0][2] * v.Z + A[0][3]);
			out->y = (A[1][0] * v.X + A[1][1] * v.Y + A[1][2] * v.Z + A[1][3]);
			out->z = (A[2][0] * v.X + A[2][1] * v.Y + A[2][2] * v.Z + A[2][3]);

			const Vector3& n=*(src_norm+vi+pidx);
			out->nx = (A[0][0] * n.X + A[0][1] * n.Y + A[0][2] * n.Z);
			out->ny = (A[1][0] * n.X + A[1][1] * n.Y + A[1][2] * n.Z);
			out->nz = (A[2][0] * n.X + A[2][1] * n.Y + A[2][2] * n.Z);

			if (diffuse) out->diffuse=diffuse[vi+pidx];
			else out->diffuse=0xFFFFFFFF;
			if (uv0) reinterpret_cast<Vector2&>(verts[vi+pidx].u1)=uv0[vi+pidx];
			else reinterpret_cast<Vector2&>(verts[vi+pidx].u2)=Vector2(0.0f,0.0f);
		}

		vi=cnt;
	}
}


// Destination pointers MUST point to arrays large enough to hold all vertices
void MeshModelClass::get_deformed_screenspace_vertices(Vector4 *dst_vert,const RenderInfoClass & rinfo,const Matrix3D & mesh_transform,const HTreeClass * htree)
{
	Matrix4x4 prj = rinfo.Camera.Get_Projection_Matrix() * rinfo.Camera.Get_View_Matrix() * mesh_transform;

	Vector3 * src_vert = Vertex->Get_Array();
	int vertex_count=Get_Vertex_Count();

	if (Get_Flag(SKIN) && VertexBoneLink && htree) {
		uint16 * bonelink = VertexBoneLink->Get_Array();
		for (int vi = 0; vi < vertex_count;) {
			int idx=bonelink[vi];

			Matrix4x4 tm = prj * htree->Get_Transform(idx);

			// Count equal matrices (the vertices should be pre-sorted by matrices they use)
			int cnt = vi;
			for (; cnt < vertex_count; cnt++) if (idx!=bonelink[cnt]) break;

			// Transform to screenspace (x,y,z,w)
			VectorProcessorClass::Transform(
				dst_vert+vi,
				src_vert+vi,
				tm,
				cnt-vi);

			vi=cnt;
		}
	} else {
		VectorProcessorClass::Transform(
			dst_vert,
			src_vert,
			prj,
			vertex_count);
	}
}

void MeshModelClass::Make_Geometry_Unique()
{
	WWASSERT(Vertex);

	ShareBufferClass<Vector3> * unique_verts = NEW_REF(ShareBufferClass<Vector3>,(*Vertex));
	REF_PTR_SET(Vertex,unique_verts);
	REF_PTR_RELEASE(unique_verts);

	ShareBufferClass<Vector3> * norms = NEW_REF(ShareBufferClass<Vector3>,(*VertexNorm));
	REF_PTR_SET(VertexNorm,norms);
	REF_PTR_RELEASE(norms);

#if (!OPTIMIZE_PLANEEQ_RAM)
	ShareBufferClass<Vector4> * peq = NEW_REF(ShareBufferClass<Vector4>,(*PlaneEq, "MeshModelClass::PlaneEq"));
	REF_PTR_SET(PlaneEq,peq);
	REF_PTR_RELEASE(peq);
#endif
}

void MeshModelClass::Make_UV_Array_Unique(int pass,int stage)
{
	CurMatDesc->Make_UV_Array_Unique(pass,stage);
}

void MeshModelClass::Make_Color_Array_Unique(int array_index)
{
	CurMatDesc->Make_Color_Array_Unique(array_index);
}

void MeshModelClass::Enable_Alternate_Material_Description(bool onoff)
{
	if ((onoff == true) && (AlternateMatDesc != nullptr)) {
		if (CurMatDesc != AlternateMatDesc) {
			CurMatDesc = AlternateMatDesc;

			if (Get_Flag(SORT) && WW3D::Is_Munge_Sort_On_Load_Enabled())
				compute_static_sort_levels();

			// TODO: Invalidate just this meshes DX8 data!!!
			TheDX8MeshRenderer.Invalidate();
		}
	} else {
		if (CurMatDesc != DefMatDesc) {
			CurMatDesc = DefMatDesc;

			if (Get_Flag(SORT) && WW3D::Is_Munge_Sort_On_Load_Enabled())
				compute_static_sort_levels();

			// TODO: Invalidate this meshes DX8 data!!!
			TheDX8MeshRenderer.Invalidate();
		}
	}
}

bool MeshModelClass::Is_Alternate_Material_Description_Enabled()
{
	return CurMatDesc == AlternateMatDesc;
}
/*
void MeshModelClass::Process_Texture_Reduction()
{
	MatInfo->Process_Texture_Reduction();
}
*/
bool MeshModelClass::Needs_Vertex_Normals()
{
	if (Get_Flag(MeshModelClass::PRELIT_MASK) == 0) {
		return true;
	}
	return CurMatDesc->Do_Mappers_Need_Normals();
}

void Whatever(
	TriIndex* added_polygon_indices,
	unsigned& added_polygon_count,
	const Vector3* locations,
	unsigned vertex_count,
	const TriIndex* polygon_indices,
	unsigned polygon_count);



struct TriangleSide
{
	Vector3 loc1;
	Vector3 loc2;
	TriangleSide(const Vector3& l1,const Vector3& l2)
	{
		int i1=*(int*)&l1[0];
		i1=37*i1+*(int*)&l1[1];
		i1=37*i1+*(int*)&l1[2];
		int i2=*(int*)&l2[0];
		i2=37*i2+*(int*)&l2[1];
		i2=37*i2+*(int*)&l2[2];
		if (i1<i2) {
			loc1=l1;
			loc2=l2;
		}
		else {
			loc2=l1;
			loc1=l2;
		}
	}
	TriangleSide() {}

	bool operator== (const TriangleSide& s)
	{
		unsigned i=*(unsigned*)&loc1[0]^*(unsigned*)&s.loc1[0];
		i|=*(unsigned*)&loc1[1]^*(unsigned*)&s.loc1[1];
		i|=*(unsigned*)&loc1[2]^*(unsigned*)&s.loc1[2];
		i|=*(unsigned*)&loc2[0]^*(unsigned*)&s.loc2[0];
		i|=*(unsigned*)&loc2[1]^*(unsigned*)&s.loc2[1];
		i|=*(unsigned*)&loc2[2]^*(unsigned*)&s.loc2[2];
		return !i;
	}
};

// Get_Hash_Value specialization for Vector3.

template <> inline unsigned int HashTemplateKeyClass<Vector3>::Get_Hash_Value(const Vector3& location)
{
	const unsigned char* buffer=(const unsigned char*)&location;
	unsigned int hval=0;
	for (unsigned int a=0;a<sizeof(Vector3);++a) {
		hval+=37*hval+buffer[a];
	}
	return hval;
}

// Get_Hash_Value specialization for TriangleSide.

template <> inline unsigned int HashTemplateKeyClass<TriangleSide>::Get_Hash_Value(const TriangleSide& side)
{
	const unsigned char* buffer=(const unsigned char*)&side;
	unsigned int hval=0;
	for (unsigned int a=0;a<sizeof(TriangleSide);++a) {
		hval+=37*hval+buffer[a];
	}
	return hval;
}

struct SideIndexInfo
{
	unsigned short vidx1;
	unsigned short vidx2;
	unsigned polygon_index;
	SideIndexInfo() {}
	SideIndexInfo(int i) { WWASSERT(0); }
};


HashTemplateClass<Vector3, unsigned> LocationHash;
HashTemplateClass<Vector3, unsigned> DuplicateLocationHash;
HashTemplateClass<TriangleSide,SideIndexInfo> SideHash;

// ----------------------------------------------------------------------------
//
// Allocate a gap-filler object. The constructor allocates memory for the
// maximum possible amount of gap polygons, which is quite much. After all
// the gap polygons have been added to the arrays, Shrink_Arrays() should
// be called to free up all unneeded memory.
//
// ----------------------------------------------------------------------------

GapFillerClass::GapFillerClass(MeshModelClass* mmc_) : mmc(nullptr), PolygonCount(0)
{
	REF_PTR_SET(mmc,mmc_);

	ArraySize=mmc->Get_Polygon_Count()*6;	// Each side of each triangle can have 2 polygons added, in the worst case
	PolygonArray=W3DNEWARRAY TriIndex[ArraySize];
	for (int pass=0;pass<mmc->Get_Pass_Count();++pass) {
		for (int stage=0;stage<MeshMatDescClass::MAX_TEX_STAGES;++stage) {
			if (mmc->Has_Texture_Array(pass,stage)) {
				TextureArray[pass][stage]=W3DNEWARRAY TextureClass*[ArraySize];
			}
			else TextureArray[pass][stage]=nullptr;
		}

		if (mmc->Has_Material_Array(pass)) {
			MaterialArray[pass]=W3DNEWARRAY VertexMaterialClass*[ArraySize];
		}
		else MaterialArray[pass]=nullptr;

		if (mmc->Has_Shader_Array(pass)) {
			ShaderArray[pass]=W3DNEWARRAY ShaderClass[ArraySize];
		}
		else ShaderArray[pass]=nullptr;
	}
}

GapFillerClass::GapFillerClass(const GapFillerClass& that) : mmc(nullptr), PolygonCount(that.PolygonCount)
{
	REF_PTR_SET(mmc,that.mmc);

	ArraySize=that.ArraySize;
	PolygonArray=W3DNEWARRAY TriIndex[ArraySize];
	for (int pass=0;pass<mmc->Get_Pass_Count();++pass) {
		for (int stage=0;stage<MeshMatDescClass::MAX_TEX_STAGES;++stage) {
			if (that.TextureArray[pass][stage]) {
				TextureArray[pass][stage]=W3DNEWARRAY TextureClass*[ArraySize];
				for (unsigned i=0;i<PolygonCount;++i) {
					TextureArray[pass][stage][i]=that.TextureArray[pass][stage][i];
					TextureArray[pass][stage][i]->Add_Ref();
				}
			}
			else TextureArray[pass][stage]=nullptr;
		}

		if (that.MaterialArray[pass]) {
			MaterialArray[pass]=W3DNEWARRAY VertexMaterialClass*[ArraySize];
			for (unsigned i=0;i<PolygonCount;++i) {
				MaterialArray[pass][i]=that.MaterialArray[pass][i];
				MaterialArray[pass][i]->Add_Ref();
			}
		}
		else MaterialArray[pass]=nullptr;

		if (that.ShaderArray[pass]) {
			ShaderArray[pass]=W3DNEWARRAY ShaderClass[ArraySize];
			for (unsigned i=0;i<PolygonCount;++i) {
				ShaderArray[pass][i]=that.ShaderArray[pass][i];
			}
		}
		else ShaderArray[pass]=nullptr;
	}
}

// ----------------------------------------------------------------------------
//
// Destruct gap-filler object. Release references to all textures and release
// the arrays.
//
// ----------------------------------------------------------------------------

GapFillerClass::~GapFillerClass()
{
	delete[] PolygonArray;

	for (int pass=0;pass<mmc->Get_Pass_Count();++pass) {
		for (int stage=0;stage<MeshMatDescClass::MAX_TEX_STAGES;++stage) {
			if (TextureArray[pass][stage]) {
				for (unsigned i=0;i<PolygonCount;++i) {
					REF_PTR_RELEASE(TextureArray[pass][stage][i]);
				}
				delete[] TextureArray[pass][stage];
			}
		}

		if (MaterialArray[pass]) {
			for (unsigned i=0;i<PolygonCount;++i) {
				REF_PTR_RELEASE(MaterialArray[pass][i]);
			}
			delete[] MaterialArray[pass];
		}

		delete[] ShaderArray[pass];
	}

	REF_PTR_RELEASE(mmc);
}

// ----------------------------------------------------------------------------
//
// Add polygon to gap filler.
//
// ----------------------------------------------------------------------------

void GapFillerClass::Add_Polygon(unsigned polygon_index,unsigned vidx1,unsigned vidx2, unsigned vidx3)
{
	WWASSERT(PolygonCount<ArraySize);
	WWASSERT(vidx1!=vidx2 && vidx1!=vidx3 && vidx2!=vidx3);
Vector3 loc1=mmc->Get_Vertex_Array()[vidx1];
Vector3 loc2=mmc->Get_Vertex_Array()[vidx2];
Vector3 loc3=mmc->Get_Vertex_Array()[vidx3];
WWASSERT(loc1==loc2 || loc1==loc3 || loc2==loc3);
//sdflksdjflsdkf
//vidx1=mmc->Get_Polygon_Array()[polygon_index][0];
//vidx2=mmc->Get_Polygon_Array()[polygon_index][1];
//vidx3=mmc->Get_Polygon_Array()[polygon_index][2];

	PolygonArray[PolygonCount]=TriIndex(vidx1,vidx2,vidx3);
	for (int pass=0;pass<mmc->Get_Pass_Count();++pass) {
		if (mmc->Has_Shader_Array(pass)) {
			ShaderArray[pass][PolygonCount]=mmc->Get_Shader(polygon_index,pass);
		}
		if (mmc->Has_Material_Array(pass)) {
//			MaterialArray[pass][PolygonCount]=mmc->Get_Material(polygon_index,pass);
			MaterialArray[pass][PolygonCount]=mmc->Get_Material(mmc->Get_Polygon_Array()[polygon_index][0],pass);
		}
		for (int stage=0;stage<MeshMatDescClass::MAX_TEX_STAGES;++stage) {
			if (mmc->Has_Texture_Array(pass,stage)) {
				TextureArray[pass][stage][PolygonCount]=mmc->Get_Texture(polygon_index,pass,stage);
			}
		}
	}
	PolygonCount++;
}

// ----------------------------------------------------------------------------
//
// Resize buffers to match the polygon count exatly. After this call no more
// polygons can be added to the buffers.
//
// ----------------------------------------------------------------------------

void GapFillerClass::Shrink_Buffers()
{
	if (PolygonCount==ArraySize) return;

	// Shrink the polygon array
	TriIndex* new_polygon_array=W3DNEWARRAY TriIndex[PolygonCount];
	memcpy(new_polygon_array,PolygonArray,PolygonCount*sizeof(TriIndex));
	delete[] PolygonArray;
	PolygonArray=new_polygon_array;

	for (int pass=0;pass<mmc->Get_Pass_Count();++pass) {
		for (int stage=0;stage<MeshMatDescClass::MAX_TEX_STAGES;++stage) {
			if (TextureArray[pass][stage]) {
				// Shrink the texture array
				TextureClass** new_texture_array=W3DNEWARRAY TextureClass*[PolygonCount];
				memcpy(new_texture_array,TextureArray[pass][stage],PolygonCount*sizeof(TextureClass*));
				delete[] TextureArray[pass][stage];
				TextureArray[pass][stage]=new_texture_array;
			}
		}

		if (MaterialArray[pass]) {
			// Shrink the material array
			VertexMaterialClass** new_material_array=W3DNEWARRAY VertexMaterialClass*[PolygonCount];
			memcpy(new_material_array,MaterialArray[pass],PolygonCount*sizeof(VertexMaterialClass*));
			delete[] MaterialArray[pass];
			MaterialArray[pass]=new_material_array;
		}

		if (ShaderArray[pass]) {
			// Shrink the shader array
			ShaderClass* new_shader_array=W3DNEWARRAY ShaderClass[PolygonCount];
			memcpy(new_shader_array,ShaderArray[pass],PolygonCount*sizeof(ShaderClass));
			delete[] ShaderArray[pass];
			ShaderArray[pass]=new_shader_array;
		}
	}
	ArraySize=PolygonCount;
}

// ----------------------------------------------------------------------------
//
// Hard edges cause gaps to n-patches meshes. This code searches for hard edges
// and adds gaps filler polygons, using existing vertices.
//
// ----------------------------------------------------------------------------

void MeshModelClass::Init_For_NPatch_Rendering()
{
	if (GapFiller) return;

	const Vector3* locations=Get_Vertex_Array();
	unsigned vertex_count=Get_Vertex_Count();
	const TriIndex* polygon_indices=Get_Polygon_Array();
	unsigned polygon_count=Get_Polygon_Count();

	LocationHash.Remove_All();
	DuplicateLocationHash.Remove_All();
	SideHash.Remove_All();

	unsigned i=0;
	for (;i<vertex_count;++i) {
		if (LocationHash.Exists(locations[i])) {
			if (!DuplicateLocationHash.Exists(locations[i])) {
				DuplicateLocationHash.Insert(locations[i],i);
			}
		}
		else {
			LocationHash.Insert(locations[i],i);
		}
	}

	for (i=0;i<polygon_count;++i) {
		bool duplicates[3];
		duplicates[0]=DuplicateLocationHash.Exists(locations[polygon_indices[i][0]]);
		duplicates[1]=DuplicateLocationHash.Exists(locations[polygon_indices[i][1]]);
		duplicates[2]=DuplicateLocationHash.Exists(locations[polygon_indices[i][2]]);
		if (duplicates[0] && duplicates[1]) {
			TriangleSide tri(locations[polygon_indices[i][0]],locations[polygon_indices[i][1]]);
			if (SideHash.Exists(tri)) {
				SideIndexInfo side_index=SideHash.Get(tri);
				unsigned idx1=side_index.vidx1;
				unsigned idx2=side_index.vidx2;
				unsigned idx3=polygon_indices[i][0];
				unsigned idx4=polygon_indices[i][1];
				bool diff=!(idx1^idx3)|!(idx1^idx4)|!(idx2^idx3)|!(idx2^idx4);
				if (!diff) {
					if (!GapFiller) GapFiller=W3DNEW GapFillerClass(this);
					GapFiller->Add_Polygon(i,idx4,idx2,idx1);
					GapFiller->Add_Polygon(side_index.polygon_index,idx3,idx2,idx4);
				}
			}
			else {
				SideIndexInfo side_index;
				side_index.vidx1=polygon_indices[i][0];
				side_index.vidx2=polygon_indices[i][1];
				side_index.polygon_index=i;
				SideHash.Insert(tri,side_index);
			}
		}
		if (duplicates[1] && duplicates[2]) {
			TriangleSide tri(locations[polygon_indices[i][1]],locations[polygon_indices[i][2]]);
			if (SideHash.Exists(tri)) {
				SideIndexInfo side_index=SideHash.Get(tri);
				unsigned idx1=side_index.vidx1;
				unsigned idx2=side_index.vidx2;
				unsigned idx3=polygon_indices[i][1];
				unsigned idx4=polygon_indices[i][2];
				bool diff=!(idx1^idx3)|!(idx1^idx4)|!(idx2^idx3)|!(idx2^idx4);
				if (!diff) {
					if (!GapFiller) GapFiller=W3DNEW GapFillerClass(this);
					GapFiller->Add_Polygon(i,idx4,idx2,idx1);
					GapFiller->Add_Polygon(side_index.polygon_index,idx3,idx2,idx4);
				}
			}
			else {
				SideIndexInfo side_index;
				side_index.vidx1=polygon_indices[i][1];
				side_index.vidx2=polygon_indices[i][2];
				side_index.polygon_index=i;
				SideHash.Insert(tri,side_index);
			}
		}
		if (duplicates[2] && duplicates[0]) {
			TriangleSide tri(locations[polygon_indices[i][2]],locations[polygon_indices[i][0]]);
			if (SideHash.Exists(tri)) {
				SideIndexInfo side_index=SideHash.Get(tri);
				unsigned idx1=side_index.vidx1;
				unsigned idx2=side_index.vidx2;
				unsigned idx3=polygon_indices[i][2];
				unsigned idx4=polygon_indices[i][0];
				bool diff=!(idx1^idx3)|!(idx1^idx4)|!(idx2^idx3)|!(idx2^idx4);
				if (!diff) {
					if (!GapFiller) GapFiller=W3DNEW GapFillerClass(this);
					GapFiller->Add_Polygon(i,idx4,idx2,idx1);
					GapFiller->Add_Polygon(side_index.polygon_index,idx3,idx2,idx4);
				}
			}
			else {
				SideIndexInfo side_index;
				side_index.vidx1=polygon_indices[i][2];
				side_index.vidx2=polygon_indices[i][0];
				side_index.polygon_index=i;
				SideHash.Insert(tri,side_index);
			}
		}
	}

	LocationHash.Remove_All();
	DuplicateLocationHash.Remove_All();
	SideHash.Remove_All();

	if (GapFiller) GapFiller->Shrink_Buffers();
}
