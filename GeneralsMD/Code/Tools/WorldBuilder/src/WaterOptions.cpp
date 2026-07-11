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

// WaterOptions.cpp : implementation file
//

#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "CUndoable.h"
#include "WaterOptions.h"
#include "WaypointOptions.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"
#include "wbview3d.h"
#include "PolygonTool.h"
#include "WaypointTool.h"
#include "GameLogic/PolygonTrigger.h"
#include "Common/WellKnownKeys.h"
#include "LayersList.h"

WaterOptions *WaterOptions::m_staticThis = nullptr;
Int WaterOptions::m_waterHeight = 7;
Int WaterOptions::m_waterPointSpacing = MAP_XY_FACTOR;
Bool WaterOptions::m_creatingWaterAreas = false;
/////////////////////////////////////////////////////////////////////////////
/// WaterOptions dialog trivial constructor - Create does the real work.


WaterOptions::WaterOptions(CWnd* pParent /*=nullptr*/):
m_moveUndoable(nullptr)
{
	//{{AFX_DATA_INIT(WaterOptions)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

/// Windows default stuff.
void WaterOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(WaterOptions)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

void WaterOptions::setHeight(Int height)
{
	char buffer[12];
	snprintf(buffer, ARRAY_SIZE(buffer), "%d", height);
	m_waterHeight = height;
	if (m_staticThis && !m_staticThis->m_updating) {
		CWnd *pEdit = m_staticThis->GetDlgItem(IDC_HEIGHT_EDIT);
		if (pEdit) pEdit->SetWindowText(buffer);
	}
}

void WaterOptions::updateTheUI()
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();

	CWnd *pWnd = this->GetDlgItem(IDC_WATERNAME_EDIT);

	if (theTrigger && pWnd) {
		pWnd->SetWindowText(theTrigger->getTriggerName().str());
		setHeight(theTrigger->getPoint(0)->z);
	}
	CButton *pButton = (CButton*)GetDlgItem(IDC_WATER_POLYGON);
	pButton->SetCheck(m_creatingWaterAreas ? 1:0);
	Bool isRiver = false;
	if (theTrigger) {
		isRiver = theTrigger->isRiver();
	}
	pButton = (CButton*)GetDlgItem(IDC_MAKE_RIVER);
	pButton->SetCheck(isRiver ? 1:0);
	pButton->EnableWindow(theTrigger!=nullptr);

	pWnd = m_staticThis->GetDlgItem(IDC_SPACING);
	char buffer[12];
	if (pWnd) {
		snprintf(buffer, ARRAY_SIZE(buffer), "%d", m_waterPointSpacing);
		pWnd->SetWindowText(buffer);
	}
}

void WaterOptions::update()
{
	if (m_staticThis) {
		m_staticThis->updateTheUI();
	}
}

/////////////////////////////////////////////////////////////////////////////
// WaterOptions message handlers

/// Dialog UI initialization.
/** Creates the slider controls, and sets the initial values for
width and feather in the ui controls. */
BOOL WaterOptions::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_updating = true;

	m_waterHeightPopup.SetupPopSliderButton(this, IDC_HEIGHT_POPUP, this);
	m_waterPointSpacing = 2*MAP_XY_FACTOR;
	m_staticThis = this;
	m_updating = false;
	setHeight(m_waterHeight);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


BEGIN_MESSAGE_MAP(WaterOptions, COptionsPanel)
	//{{AFX_MSG_MAP(WaterOptions)
	ON_CBN_KILLFOCUS(IDC_WATERNAME_EDIT, OnChangeWaterEdit)
	ON_EN_CHANGE(IDC_HEIGHT_EDIT, OnChangeHeightEdit)
	ON_EN_CHANGE(IDC_SPACING, OnChangeSpacingEdit)
	ON_BN_CLICKED(IDC_WATER_POLYGON, OnWaterPolygon)
	ON_BN_CLICKED(IDC_MAKE_RIVER, OnMakeRiver)
	ON_CBN_SELENDOK(IDC_WATERNAME_EDIT, OnChangeWaterEdit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void WaterOptions::OnChangeWaterEdit()
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();

	// get the combo box
	CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_WATERNAME_EDIT);
	if (pCombo) {
		// get the text out of the combo. If it is user-typed, sel will be -1, otherwise it will be >=0
		CString theText;
		Int sel = pCombo->GetCurSel();
		if (sel >= 0) {
			pCombo->GetLBText(sel, theText);
		} else {
			pCombo->GetWindowText(theText);
		}
		AsciiString name((LPCTSTR)theText);

		// check to see if the user-entered name is already in use.
		Bool didMatch = false;

		// check trigger area objects
		PolygonTrigger *pTrig;
		for (pTrig=PolygonTrigger::getFirstPolygonTrigger(); !didMatch && pTrig; pTrig = pTrig->getNext()) {
			if (pTrig==theTrigger) continue; // don't check against yourself.
			const AsciiString& trigName = pTrig->getTriggerName();
			if (name == trigName) {
				if (pTrig->isValid()) {
					didMatch = true;
				} else {
					PolygonTrigger::removePolygonTrigger(pTrig);
				}
				break;
			}
		}

		// if there's a match, throw up a messagebox, otherwise set the name
		if (didMatch) {
			::AfxMessageBox("Name already in use");
		} else {
			if (theTrigger) {
				theTrigger->setTriggerName(name);
			}
		}
	}
}


void WaterOptions::OnWaterPolygon()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_WATER_POLYGON);
	m_creatingWaterAreas = (pButton->GetCheck()==1);
}

void WaterOptions::OnMakeRiver()
{
	CButton *pButton = (CButton*)GetDlgItem(IDC_MAKE_RIVER);
	Bool river = (pButton->GetCheck()==1);
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (theTrigger) {
		theTrigger->setRiver(river);
		if (river) {
			Int curPoint = PolygonTool::getSelectedPointNdx();
			if (curPoint >= 0) {
				Real endLen=0;
				Int newPoint = curPoint;
				if (curPoint>0) curPoint--;
				if (curPoint>0) curPoint--;
				Int i;
				for (i=curPoint; i<theTrigger->getNumPoints()-1 && i<curPoint+4; i++) {
					ICoord3D innerPt = *theTrigger->getPoint(i);
					ICoord3D outerPt = *theTrigger->getPoint(i+1);
					Real dx = innerPt.x-outerPt.x;
					Real dy = innerPt.y-outerPt.y;
					Real curLen = sqrt(dx*dx+dy*dy);
					if ( curLen>endLen) {
						newPoint = i;
						endLen = curLen;
					}
				}
				theTrigger->setRiverStart(newPoint);

				// Now find the other end.
//				Real sourceWidth = endLen;

				endLen=0;
				Int endPoint = 0;
				for (i=0; i<theTrigger->getNumPoints()-1; i++) {
					if (i>=newPoint-1 && i<=newPoint+1) continue;
					ICoord3D innerPt = *theTrigger->getPoint(i);
					ICoord3D outerPt = *theTrigger->getPoint(i+1);
					Real dx = innerPt.x-outerPt.x;
					Real dy = innerPt.y-outerPt.y;
					Real curLen = sqrt(dx*dx+dy*dy);
					if ( curLen>endLen) {
						endPoint = i;
						endLen = curLen;
					}
				}
				Int pointsOut = endPoint - newPoint;
				Int pointsIn = 	newPoint - endPoint;
				if (pointsOut<0) pointsOut += theTrigger->getNumPoints();
				if (pointsIn<0) pointsIn += theTrigger->getNumPoints();
				Int delta = pointsIn-pointsOut;
				if (delta<0) delta = -delta;
				if (delta>1) {
					PolygonTrigger *pNew;
					if (pointsOut<pointsIn) {
						pNew = adjustCount(theTrigger, newPoint+1, endPoint, pointsIn-1);
						theTrigger->setRiverStart(pointsIn);
					}	else {
						pNew = adjustCount(theTrigger, endPoint+1, newPoint, pointsOut-1);
						theTrigger->setRiverStart(0);
					}
					while(theTrigger->getNumPoints()) theTrigger->deletePoint(theTrigger->getNumPoints()-1);
					for (i=0; i<pNew->getNumPoints(); i++) {
						theTrigger->addPoint(*pNew->getPoint(i));
					}
					deleteInstance(pNew);
				}
			}
		}
	}
}

/// Adjust the spacing.
PolygonTrigger * WaterOptions::adjustCount(PolygonTrigger *trigger, Int firstPt, Int lastPt, Int desiredPointCount)
{
	PolygonTrigger *pNew = newInstance(PolygonTrigger)(trigger->getNumPoints());
//	Real endLen=0;
	Real totalLen=0;
	Real curSpacingLen = 10;
	Int curPoint = lastPt;
	ICoord3D pt;
	while (curPoint != firstPt) {
		pt = *trigger->getPoint(curPoint);
		pNew->addPoint(pt);
		curPoint++;
		if (curPoint>=trigger->getNumPoints()) {
			curPoint = 0;
		}
	}

	curPoint = firstPt;
	while (curPoint != lastPt) {
		Int nextPoint = curPoint;
		nextPoint++;
		if (nextPoint>=trigger->getNumPoints()) {
			nextPoint = 0;
		}
		ICoord3D curPt = *trigger->getPoint(curPoint);
		ICoord3D nextPt = *trigger->getPoint(nextPoint);
		Real dx = nextPt.x-curPt.x;
		Real dy = nextPt.y-curPt.y;
		Real curLen = sqrt(dx*dx+dy*dy);
		totalLen += curLen;
		curPoint = nextPoint;
	}
	Real spacing = totalLen/(desiredPointCount-1);

	Bool didCurPoint = true;

	curPoint = firstPt;
	pt = *trigger->getPoint(curPoint);
	pNew->addPoint(pt);
	while (curPoint != lastPt) {
		Int nextPoint = curPoint;
		nextPoint++;
		if (nextPoint>=trigger->getNumPoints()) {
			nextPoint = 0;
		}
		ICoord3D curPt = *trigger->getPoint(curPoint);
		ICoord3D nextPt = *trigger->getPoint(nextPoint);
		Real dx = nextPt.x-curPt.x;
		Real dy = nextPt.y-curPt.y;
		Real curLen = sqrt(dx*dx+dy*dy);
		if (curLen > 4*MAP_XY_FACTOR && curLen>2*spacing) {
			if (!didCurPoint) pNew->addPoint(curPt);
			else pNew->setPoint(curPt, pNew->getNumPoints()-1);
			pNew->addPoint(nextPt);
			didCurPoint = true;
			curSpacingLen = spacing;
		} else if (curSpacingLen>curLen) {
			curSpacingLen -= curLen;
		} else {
			while (curLen >= curSpacingLen) {
				// cur len > curSpacingLen.
				Real factor = curSpacingLen/curLen;
				curPt.x += dx*factor;
				curPt.y += dy*factor;
				pNew->addPoint(curPt);
				didCurPoint = false;
				dx = nextPt.x-curPt.x;
				dy = nextPt.y-curPt.y;
				curLen -= curSpacingLen;
				curSpacingLen = spacing;
			}
			curSpacingLen -= curLen;
 			if ((curLen)<MAP_XY_FACTOR/2) {
				didCurPoint = true;
			}
		}
		curPoint = nextPoint;
	}
	return pNew;
}

void WaterOptions::GetPopSliderInfo(const long sliderID, long *pMin, long *pMax, long *pLineSize, long *pInitial)
{
	switch (sliderID) {


		case IDC_HEIGHT_POPUP:
			*pMin = 0;
			*pMax = 255*MAP_HEIGHT_SCALE;
			*pInitial = m_waterHeight;
			*pLineSize = 1;
			break;

		default:
			// uh-oh!
			DEBUG_CRASH(("Slider message from unknown control"));
			break;
	}
}

void WaterOptions::PopSliderChanged(const long sliderID, long theVal)
{
	CString str;
	CWnd *pEdit;
	switch (sliderID) {


		case IDC_HEIGHT_POPUP:
			m_waterHeight = theVal;
			str.Format("%d",m_waterHeight);
			pEdit = m_staticThis->GetDlgItem(IDC_HEIGHT_EDIT);
			m_updating = true;
			if (pEdit) pEdit->SetWindowText(str);
			startUpdateHeight();
			updateHeight();
			m_updating = false;
			break;


		default:
			// uh-oh!
			DEBUG_CRASH(("Slider message from unknown control"));
			break;
	}
}

void WaterOptions::PopSliderFinished(const long sliderID, long theVal)
{
	switch (sliderID) {
		case IDC_HEIGHT_POPUP:
			updateHeight();
			endUpdateHeight();
			break;

		default:
			// uh-oh!
			DEBUG_CRASH(("Slider message from unknown control"));
			break;
	}

}

void WaterOptions::startUpdateHeight()
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (!theTrigger) {
		REF_PTR_RELEASE(m_moveUndoable);
		return;
	}
	if (!theTrigger->isWaterArea()) {
		REF_PTR_RELEASE(m_moveUndoable);
		return;
	}
	if (m_moveUndoable && theTrigger == m_moveUndoable->getTrigger()) {
		return;
	}
	m_originalHeight = theTrigger->getPoint(0)->z;
	Int i;
	for (i=0; i<theTrigger->getNumPoints(); i++) {
		ICoord3D loc = *theTrigger->getPoint(i);
		loc.z = m_originalHeight;
		theTrigger->setPoint(loc, i);
	}
	m_moveUndoable = new MovePolygonUndoable(theTrigger);
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	pDoc->AddAndDoUndoable(m_moveUndoable);
}


void WaterOptions::updateHeight()
{
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	if (!theTrigger || !m_moveUndoable) {
		REF_PTR_RELEASE(m_moveUndoable); // belongs to pDoc now.
		return;
	}
	if (!theTrigger->isWaterArea()) {
		REF_PTR_RELEASE(m_moveUndoable); // belongs to pDoc now.
		return;
	}
	if (theTrigger != m_moveUndoable->getTrigger()) {
		REF_PTR_RELEASE(m_moveUndoable); // belongs to pDoc now.
		return;
	}
	ICoord3D iLoc;
	Int dz = m_waterHeight - m_originalHeight;
	iLoc.x = 0;
	iLoc.y = 0;
	iLoc.z = dz;
	m_moveUndoable->SetOffset(iLoc);
	WbView3d *pView = CWorldBuilderDoc::GetActive3DView();
	pView->Invalidate();
}

void WaterOptions::endUpdateHeight()
{
	REF_PTR_RELEASE(m_moveUndoable); // belongs to pDoc now.
}


 /// Handles width edit ui messages.
/** Gets the new edit control text, converts it to an int, then updates
		the slider and brush tool. */
void WaterOptions::OnChangeHeightEdit()
{
	if (m_updating) return;
	CWnd *pEdit = m_staticThis->GetDlgItem(IDC_HEIGHT_EDIT);
	char buffer[_MAX_PATH];
	if (pEdit) {
		pEdit->GetWindowText(buffer, sizeof(buffer));
		Int height;
		m_updating = true;
		if (1==sscanf(buffer, "%d", &height)) {
			m_waterHeight = height;
			startUpdateHeight();
			updateHeight();
			endUpdateHeight();
		}
		m_updating = false;
	}
}

 /// Handles width edit ui messages.
/** Gets the new edit control text, converts it to an int, then updates
		the slider and brush tool. */
void WaterOptions::OnChangeSpacingEdit()
{
	if (m_updating) return;
	CWnd *pEdit = m_staticThis->GetDlgItem(IDC_SPACING);
	char buffer[12];
	if (pEdit) {
		pEdit->GetWindowText(buffer, sizeof(buffer));
		Int height;
		m_updating = true;
		if (1==sscanf(buffer, "%d", &height)) {
			m_waterPointSpacing = height;
		}	else {
			snprintf(buffer, ARRAY_SIZE(buffer), "%d", m_waterPointSpacing);
			pEdit->SetWindowText(buffer);
		}
		m_updating = false;
	}
}

