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


// WorldHeightMap.h
// Class to encapsulate height map.
// Author: John Ahlquist, April 2001

#pragma once

#include "Lib/BaseType.h"
#include "WWLib/refcount.h"
#include "WWMath/vector3.h"
#include "W3DDevice/GameClient/TileData.h"
#include "Common/MapObject.h"

#include "Common/STLTypedefs.h"
typedef std::vector<ICoord2D> VecICoord2D;


/** MapObject class
Not ref counted.  Do not store pointers to this class.  */

#define VERTEX_BUFFER_TILE_LENGTH 32 //tiles of side length 32 (grid of 33x33 vertices).
#define VERTS_IN_BLOCK_ROW (VERTEX_BUFFER_TILE_LENGTH + 1)
#define HEIGHTMAP_VERTEX_NUM (VERTEX_BUFFER_TILE_LENGTH * 2 * VERTEX_BUFFER_TILE_LENGTH * 2)
#define HEIGHTMAP_POLYGON_NUM (VERTEX_BUFFER_TILE_LENGTH * VERTEX_BUFFER_TILE_LENGTH * 2)

#define K_MIN_HEIGHT  0
#define K_MAX_HEIGHT  255

#define NUM_SOURCE_TILES 1024
#define NUM_BLEND_TILES 16192
#define NUM_CLIFF_INFO 32384
#define FLAG_VAL 0x7ADA0000

// For backwards compatiblity.
#define TEX_PATH_LEN 256


/// Struct in memory.
typedef struct {
	Int globalTextureClass;
	Int firstTile;
	Int numTiles;
	Int width;
	Int isBlendEdgeTile; ///< True if the texture contains blend edges.
	AsciiString name;
	ICoord2D positionInTexture;
} TXTextureClass;

typedef enum {POS_X, POS_Y, NEG_X, NEG_Y} TVDirection;
/// Struct in memory.
typedef struct {
	Real	u0, v0;	 // Upper left uv
	Real	u1, v1;	 // Lower left uv
	Real	u2, v2;	 // Lower right uv
	Real	u3, v3;	 // Upper right uv
	Bool  flip;
	Bool	mutant;  // Mutant mapping needed to get this to fit.
	Short tileIndex; // Tile texture.
} TCliffInfo;

#define NUM_TEXTURE_CLASSES 256


class TextureClass;
class ChunkInputStream;
class InputStream;
class OutputStream;
class DataChunkInput;
struct DataChunkInfo;
class TerrainTextureClass;
class AlphaTerrainTextureClass;
class AlphaEdgeTextureClass;

#define NUM_ALPHA_TILES 12

class WorldHeightMap : public RefCountClass,
                       public WorldHeightMapInterfaceClass
{
	friend class TerrainTextureClass;
	friend class AlphaTerrainTextureClass;
	friend class W3DCustomEdging;
	friend class AlphaEdgeTextureClass;

#define NO_EVAL_TILING_MODES

public:

	struct DrawArea
	{
		Int originX;
		Int originY;
		Int sizeX;
		Int sizeY;
	};

#ifdef EVAL_TILING_MODES
	enum {TILE_4x4, TILE_6x6, TILE_8x8} m_tileMode;
#endif
	enum {
		NORMAL_DRAW_WIDTH = 1 + 4*VERTEX_BUFFER_TILE_LENGTH,
		NORMAL_DRAW_HEIGHT = 1 + 4*VERTEX_BUFFER_TILE_LENGTH,
		STRETCH_DRAW_WIDTH = 1 + 2*VERTEX_BUFFER_TILE_LENGTH,
		STRETCH_DRAW_HEIGHT = 1 + 2*VERTEX_BUFFER_TILE_LENGTH,
		LOW_ANGLE_DRAW_WIDTH = 1 + (NORMAL_DRAW_WIDTH-1) * 2,
		LOW_ANGLE_DRAW_HEIGHT = 1 + (NORMAL_DRAW_HEIGHT-1) * 2,
	};

protected:
	Int m_width;				///< Height map width.
	Int m_height;				///< Height map height (y size of array).
	Int m_borderSize;		///< Non-playable border area.
	VecICoord2D m_boundaries;	///< the in-game boundaries
	Int m_dataSize;			///< size of m_data.
	UnsignedByte *m_data;	///< array of z(height) values in the height map.

  UnsignedByte *m_seismicUpdateFlag;  ///< array of bits to prevent ovelapping physics-update regions from doubling effects on shared cells
  UnsignedInt   m_seismicUpdateWidth; ///< width of the array holding SeismicUpdateFlags
  Real         *m_seismicZVelocities; ///< how fast is the dirt rising/falling at this location

  UnsignedByte *m_cellFlipState;	///< array of bits to indicate the flip state of each cell.
	Int m_flipStateWidth;			///< with of the array holding cellFlipState
	UnsignedByte *m_cellCliffState;	///< array of bits to indicate the cliff state of each cell.


	/// Texture indices.
	Short  *m_tileNdxes;  ///< matches m_Data, indexes into m_SourceTiles.
	Short  *m_blendTileNdxes;  ///< matches m_Data, indexes into m_blendedTiles.  0 means no blend info.
	Short  *m_cliffInfoNdxes;  ///< matches m_Data, indexes into m_cliffInfo.	 0 means no cliff info.
	Short  *m_extraBlendTileNdxes;  ///< matches m_Data, indexes into m_extraBlendedTiles.  0 means no blend info.


	Int m_numBitmapTiles;	// Number of tiles initialized from bitmaps in m_SourceTiles.
	Int m_numEdgeTiles;	// Number of tiles initialized from bitmaps in m_SourceTiles.
	Int m_numBlendedTiles;	// Number of blended tiles created from bitmap tiles.

	TileData			*m_sourceTiles[NUM_SOURCE_TILES];	///< Tiles for m_textureClasses
	TileData			*m_edgeTiles[NUM_SOURCE_TILES];	///< Tiles for m_textureClasses

	TBlendTileInfo	m_blendedTiles[NUM_BLEND_TILES];
	TBlendTileInfo	m_extraBlendedTiles[NUM_BLEND_TILES];

	TCliffInfo	m_cliffInfo[NUM_CLIFF_INFO];
	Int m_numCliffInfo; ///< Number of cliffInfo's used in m_cliffInfo.

	// Texture classes.  There is one texture class for each bitmap read in.
	// A class may have more than one tile.  For example, if the grass bitmap is
	// 128x128, it creates 4 64x64 tiles, so the grass texture class will have 4 tiles.
	int m_numTextureClasses;
	TXTextureClass m_textureClasses[NUM_TEXTURE_CLASSES];

	// Edge Texture classes.  There is one texture class for each bitmap read in.
	// An edge class will normally have 4 tiles.
	int m_numEdgeTextureClasses;
	TXTextureClass m_edgeTextureClasses[NUM_TEXTURE_CLASSES];

	/** The actual texture used to render the 3d mesh.  Note that it is
	 basically m_SourceTiles laid out in rows, so by itself it is not useful.
	 Use GetUVData to get the mapping info for height cells to map into the
	 texture. */
	TerrainTextureClass *m_terrainTex;
	Int	m_terrainTexHeight; /// Height of m_terrainTex allocated.
	/** The texture that contains the alpha edge tiles that get blended on
			top of the base texture. getAlphaUVData does the mapping. */
	AlphaTerrainTextureClass *m_alphaTerrainTex;
	Int	m_alphaTexHeight; /// Height of m_alphaTerrainTex allocated.

	/** The texture that contains custom blend edge tiles. */
	AlphaEdgeTextureClass *m_alphaEdgeTex;
	Int	m_alphaEdgeHeight; /// Height of m_alphaEdgeTex allocated.

	/// Drawing info - re the part of the map that is being drawn.
	Int m_drawOriginX;
	Int m_drawOriginY;
	Int m_drawWidthX;
	Int m_drawHeightY;

	/// Tiles that hold the alpha channel info.
	static TileData *m_alphaTiles[NUM_ALPHA_TILES];


protected:
	TileData *getSourceTile(UnsignedInt ndx) { if (ndx<NUM_SOURCE_TILES) return(m_sourceTiles[ndx]); return(nullptr); };
	TileData *getEdgeTile(UnsignedInt ndx) { if (ndx<NUM_SOURCE_TILES) return(m_edgeTiles[ndx]); return(nullptr); };
	/// UV mapping data for a cell to map into the terrain texture.
	void getUVForNdx(Int ndx, float *minU, float *minV, float *maxU, float*maxV);
	Bool getUVForTileIndex(Int ndx, Short tileNdx, float U[4], float V[4]);
	Int getTextureClassFromNdx(Int tileNdx);
	void readTexClass(TXTextureClass *texClass, TileData **tileData);
	Int updateTileTexturePositions(Int *edgeHeight); ///< Places each tile in the texture.
	void initCliffFlagsFromHeights();
	void setCellCliffFlagFromHeights(Int xIndex, Int yIndex);

protected:	 // file reader callbacks.
	static Bool ParseHeightMapDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	Bool ParseHeightMapData(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParseSizeOnlyInChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	Bool ParseSizeOnly(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParseBlendTileDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	Bool ParseBlendTileData(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParseWorldDictDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParseObjectsDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	static Bool ParseObjectDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);
	Bool ParseObjectData(DataChunkInput &file, DataChunkInfo *info, void *userData, Bool readDict);
	static Bool ParseLightingDataChunk(DataChunkInput &file, DataChunkInfo *info, void *userData);

protected:
	WorldHeightMap();			///< Simple constructor for WorldHeightMapEdit class.

public: // constructors/destructors
	WorldHeightMap(ChunkInputStream *pFile, Bool bHMapOnly=false);	// read from file.
	virtual ~WorldHeightMap() override;			// destroy.

public:  // Boundary info
	const VecICoord2D& getAllBoundaries() const { return m_boundaries; }

public:  // height map info.
	static Int getMinHeightValue() {return K_MIN_HEIGHT;}
	static Int getMaxHeightValue() {return K_MAX_HEIGHT;}

	UnsignedByte *getDataPtr() {return m_data;}


	Int getXExtent() {return m_width;}	///<number of vertices in x
	Int getYExtent() {return m_height;}	///<number of vertices in y

	Region2D getDrawRegion2D();

	Int getDrawOrgX() {return m_drawOriginX;}
	Int getDrawOrgY() {return m_drawOriginY;}

	Int getDrawWidth() {return m_drawWidthX;}
	Int getDrawHeight() {return m_drawHeightY;}
	void setDrawWidth(Int width) {DEBUG_ASSERTCRASH(width <= m_width, ("Draw width must not exceed map width")); m_drawWidthX = width;}
	void setDrawHeight(Int height) {DEBUG_ASSERTCRASH(height <= m_height, ("Draw height must not exceed map height")); m_drawHeightY = height;}
	virtual Int getBorderSize() override {return m_borderSize;}
  Int getBorderSizeInline() const { return m_borderSize; }
	/// Get height with the offset that HeightMapRenderObjClass uses built in.
	UnsignedByte getDisplayHeight(Int x, Int y) { return m_data[x+m_drawOriginX+m_width*(y+m_drawOriginY)];}

	/// Get height in normal coordinates.
	UnsignedByte getHeight(Int xIndex, Int yIndex)
	{
		Int ndx = (yIndex*m_width)+xIndex;
		if ((ndx>=0) && (ndx<m_dataSize) && m_data)
			return(m_data[ndx]);
		else
			return(0);
	};

	void getUVForBlend(Int edgeClass, Region2D *range);

	DrawArea createDrawArea(Int xOrg, Int yOrg);
	Bool setDrawArea(const DrawArea& drawArea);
	Bool setDrawOrg(Int xOrg, Int yOrg);

	static void freeListOfMapObjects();

	Int getTextureClassNoBlend(Int xIndex, Int yIndex, Bool baseClass=false);
	Int getTextureClass(Int xIndex, Int yIndex, Bool baseClass=false);
	TXTextureClass getTextureFromIndex( Int textureIndex );

public:  // tile and texture info.
	void setTextureLOD(Int lod);	///< set maximum lod level sent to the hardware.
	TextureClass *getTerrainTexture();  //< generates if needed and returns the terrain texture
	TextureClass *getAlphaTerrainTexture(); //< generates if needed and returns alpha terrain texture
	TextureClass *getEdgeTerrainTexture(); //< generates if needed and returns blend edge texture
	/// UV mapping data for a cell to map into the terrain texture.  Returns true if the textures had to be stretched for cliffs.
	Bool getUVData(Int xIndex, Int yIndex, float U[4], float V[4]);
	Bool getFlipState(Int xIndex, Int yIndex) const;
	///Faster version of above function without all the safety checks - For people that do checks externally.
	Bool getQuickFlipState(Int xIndex, Int yIndex) const
	{
		return m_cellFlipState[yIndex*m_flipStateWidth + (xIndex >> 3)] & (1<<(xIndex&0x7));
	}

	void setFlipState(Int xIndex, Int yIndex, Bool value);
	void clearFlipStates();
	Bool getCliffState(Int xIndex, Int yIndex) const;
	Bool getExtraAlphaUVData(Int xIndex, Int yIndex, float U[4], float V[4], UnsignedByte alpha[4], Bool *flip, Bool *cliff);
	/// UV mapping data for a cell to map into the alpha terrain texture.
	void getAlphaUVData(Int xIndex, Int yIndex, float U[4], float V[4], UnsignedByte alpha[4], Bool *flip);
	void getTerrainColorAt(Real x, Real y, RGBColor *pColor);
	AsciiString getTerrainNameAt(Real x, Real y);
	Bool isCliffMappedTexture(Int xIndex, Int yIndex);


  Bool getSeismicUpdateFlag(Int xIndex, Int yIndex) const;
  void setSeismicUpdateFlag(Int xIndex, Int yIndex, Bool value);
  void clearSeismicUpdateFlags() ;
  virtual Real getSeismicZVelocity(Int xIndex, Int yIndex) const override;
  virtual void setSeismicZVelocity(Int xIndex, Int yIndex, Real value) override;
  void fillSeismicZVelocities( Real value );
  virtual Real getBilinearSampleSeismicZVelocity( Int x, Int y) override;



public:  // Flat tile texture info.
	TerrainTextureClass *getFlatTexture(Int xCell, Int yCell, Int cellWidth, Int pixelsPerCell);  //< generates and returns the terrain texture

	static void setupAlphaTiles();
	UnsignedByte *getPointerToTileData(Int xIndex, Int yIndex, Int width);
	Bool getRawTileData(Short tileNdx, Int width, UnsignedByte *buffer, Int bufLen);
	UnsignedByte *getRGBAlphaDataForWidth(Int width, TBlendTileInfo *pBlend);

public:  // modify height value
	void setRawHeight(Int xIndex, Int yIndex, UnsignedByte height) {
		Int ndx = (yIndex*m_width)+xIndex;
		if ((ndx>=0) && (ndx<m_dataSize) && m_data) m_data[ndx]=height;
	};
public: // Read tile utilities. jba [7/9/2003]
	static Bool readTiles(InputStream *pStrm, TileData **tiles, Int numRows);
	static Int countTiles(InputStream *pStrm, Bool *halfTile=nullptr);

protected:
	void setCliffState(Int xIndex, Int yIndex, Bool state);

};
