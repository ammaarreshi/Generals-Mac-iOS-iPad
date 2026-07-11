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

// FILE: Smudge.h /////////////////////////////////////////////////////////

#pragma once

#include <Utility/hash_map_adapter.h>
#include "WW3D2/dllist.h"
#include "WWMath/vector2.h"
#include "WWMath/vector3.h"

#define SET_SMUDGE_PARAMETERS(smudge,pos,offset,size,opacity) (smudge->m_pos=pos;smudge->m_offset=offset;smudge->m_size=size;smudge->m_opacity=opacity;)

struct Smudge : public DLNodeClass<Smudge>
{
	typedef void *Identifier;

	W3DMPO_CODE(Smudge)

	Identifier m_identifier;	//a number or pointer to identify this smudge
	Vector3 m_pos;	//position of smudge center
	Vector2 m_offset; // difference in position between "texture" extraction and re-insertion for center vertex
	Real m_size;		//size of smudge in world space.
	Real m_opacity;	//alpha of center vertex, corners are assumed at 0
	Bool m_draw;	//whether this smudge needs to be drawn

	struct smudgeVertex
	{
		Vector3 pos;	//world-space position of vertex
		Vector2 uv;	//uv coordinates of vertex
	};
	smudgeVertex m_verts[5];	//5 vertices of this smudge (in counter-clockwise order, starting at top-left, ending in center.)
};

#ifdef USING_STLPORT
namespace std
{
	template<> struct hash<Smudge::Identifier>
	{
		size_t operator()(Smudge::Identifier id) const { return reinterpret_cast<size_t>(id); }
	};
}
#endif // USING_STLPORT

struct SmudgeSet : public DLNodeClass<SmudgeSet>
{
	friend class SmudgeManager;

	W3DMPO_CODE(SmudgeSet)

	SmudgeSet();
	~SmudgeSet();

	void reset();
	void resetDraw();

	Smudge *addSmudgeToSet(Smudge::Identifier identifier); ///< add and return a smudge to the set with the given identifier
	void removeSmudgeFromSet(Smudge *&smudge); ///< remove and invalidate the given smudge
	Smudge *findSmudge(Smudge::Identifier identifier); ///< find the smudge that belongs to this identifier

	DLListClass<Smudge> &getUsedSmudgeList() { return m_usedSmudgeList;}
	Int getUsedSmudgeCount() { return m_usedSmudgeCount; }	///<active smudges that need rendering.

private:
	typedef std::hash_map<Smudge::Identifier, Smudge *> SmudgeIdToPtrMap;

	DLListClass<Smudge> m_usedSmudgeList;	///<list of smudges in this set.
	SmudgeIdToPtrMap m_usedSmudgeMap;
	static DLListClass<Smudge> m_freeSmudgeList;	///<list of unused smudges for use by SmudgeSets.
	Int m_usedSmudgeCount;
};

class SmudgeManager
{
public:
	SmudgeManager();
	virtual ~SmudgeManager();

	virtual void init();
	virtual void reset ();
	virtual void ReleaseResources() {}
	virtual void ReAcquireResources() {}

	void resetDraw(); ///< reset whether all smudges need to be drawn

	SmudgeSet *addSmudgeSet(); ///< add and return a new smudge set
	void removeSmudgeSet(SmudgeSet *&smudgeSet); ///< remove and invalidate the given smudge set
	Smudge *findSmudge(Smudge::Identifier identifier); ///< find the smudge from any smudge set
	Int getSmudgeCountLastFrame() {return m_smudgeCountLastFrame;} ///<return number of smudges submitted last frame.
	Bool getHardwareSupport() { return m_hardwareSupportStatus != SMUDGE_SUPPORT_NO;}

protected:

	enum HardwareSmudgeSupport {SMUDGE_SUPPORT_UNKNOWN,SMUDGE_SUPPORT_NO,SMUDGE_SUPPORT_YES};

	HardwareSmudgeSupport m_hardwareSupportStatus;///< flag whether we verified that the effect is supported by hardware.

	DLListClass<SmudgeSet> m_usedSmudgeSetList;	///<used SmudgeSets
	DLListClass<SmudgeSet> m_freeSmudgeSetList;	///<unused SmudgeSets ready for re-use.
	Int m_smudgeCountLastFrame;	//number of total smudges in manager last frame.
};

extern SmudgeManager *TheSmudgeManager;	///<singleton
