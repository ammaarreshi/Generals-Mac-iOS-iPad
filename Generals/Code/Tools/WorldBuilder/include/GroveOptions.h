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

#pragma once

#include <map>
#include <vector>
#include "WorldBuilder.h"
#include "OptionsPanel.h"
#include "Common/STLTypedefs.h"

// Used to store Display names in conjunction with internal names.
typedef std::pair<AsciiString, UnicodeString>							PairNameDisplayName;
typedef std::vector<PairNameDisplayName>									VecPairNameDisplayName;
typedef std::vector<PairNameDisplayName>::iterator				VecPairNameDisplayNameIt;


// This is a utility function useful to get a display string from a pair of AsciiString
// UnicodeStrings. It attempts to use the UnicodeString, and if that fails then turns
// to the AsciiString
// As a last resort, it returns the EmptyString.
UnicodeString GetDisplayNameFromPair(const PairNameDisplayName *pNamePair);

class GroveOptions : public COptionsPanel
{
	protected:
		std::vector<std::pair<Int, Int>/**/>	mVecGroup;
		VecPairNameDisplayName mVecDisplayNames;

		Int	mNumTrees;

	public:
		GroveOptions(CWnd* pParent = nullptr);
		virtual ~GroveOptions() override;
		void makeMain();

		virtual BOOL OnInitDialog() override;
		int getNumTrees();
		int getNumType(int type);
		AsciiString getTypeName(int type);
		int getTotalTreePerc();
		Bool getCanPlaceInWater();
		Bool getCanPlaceOnCliffs();

	protected:
		void _setTreesToLists();
		void _buildTreeList();
		void _setDefaultRatios();
		void _setDefaultNumTrees();
		void _setDefaultPlacementAllowed();

		afx_msg void _updateTreeWeights();
		afx_msg void _updateTreeCount();
		afx_msg void _updateGroveMakeup();
		afx_msg void _updatePlacementAllowed();

		virtual void OnOK() override;
		virtual void OnClose();
	DECLARE_MESSAGE_MAP()
};

extern GroveOptions *TheGroveOptions;
