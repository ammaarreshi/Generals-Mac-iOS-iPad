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

// WaypointOptions.cpp : implementation file
//

#include "StdAfx.h"
#include "resource.h"
#include "Lib/BaseType.h"
#include "CUndoable.h"
#include "WaypointOptions.h"
#include "WorldBuilder.h"
#include "WorldBuilderDoc.h"
#include "wbview3d.h"
#include "PolygonTool.h"
#include "WaypointTool.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/Scripts.h"
#include "Common/WellKnownKeys.h"
#include "LayersList.h"

WaypointOptions *WaypointOptions::m_staticThis = nullptr;
/////////////////////////////////////////////////////////////////////////////
/// WaypointOptions dialog trivial constructor - Create does the real work.


WaypointOptions::WaypointOptions(CWnd* pParent /*=nullptr*/):
m_moveUndoable(nullptr)
{
	//{{AFX_DATA_INIT(WaypointOptions)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

/// Windows default stuff.
void WaypointOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(WaypointOptions)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

MapObject *WaypointOptions::getSingleSelectedWaypoint()
{
	MapObject *theMapObj = nullptr;
//	Bool found = false;
	Int selCount=0;
	MapObject *pMapObj;
	for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
		if (pMapObj->isSelected()) {
			if (pMapObj->isWaypoint()) {
				theMapObj = pMapObj;
			}
			selCount++;
		}
	}
	if (selCount==1 && theMapObj) {
		return theMapObj;
	}

	return(nullptr);
}

PolygonTrigger *WaypointOptions::getSingleSelectedPolygon()
{
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc==nullptr) return nullptr;
	WbView3d *p3View = pDoc->GetActive3DView();
	Bool showPoly = false;
	if (p3View) {
		showPoly = p3View->isPolygonTriggerVisible();
	}
	if (showPoly || PolygonTool::isActive()) {
		for (PolygonTrigger *pTrig=PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
			if (PolygonTool::isSelected(pTrig)) {
				return pTrig;
			}
		}
	}
	return(nullptr);
}

void WaypointOptions::updateTheUI()
{
	Tool *curTool = ((CWorldBuilderApp*)AfxGetApp())->getCurTool();

	Bool isWaypointTool = (curTool && (curTool->getToolID() == ID_WAYPOINT_TOOL));
	MapObject *theMapObj = getSingleSelectedWaypoint();
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();

	CWnd *pWnd = this->GetDlgItem(IDC_WAYPOINTNAME_EDIT);
	CWnd *pCaption1 = this->GetDlgItem(IDC_WAYPOINT_CAPTION1);
	CWnd *pCaption2 = this->GetDlgItem(IDC_WAYPOINT_PATHLABELS);
	CWnd *pCaption3 = this->GetDlgItem(65535);
	CWnd *pCaption4 = this->GetDlgItem(65534);
	CWnd *pCaption5 = this->GetDlgItem(IDC_LIST_WAYPOINTS);

	CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_WAYPOINTNAME_EDIT);
	CComboBox *pListWayptNames = (CComboBox*)GetDlgItem(IDC_LIST_OF_WAYPOINT_NAMES);

	CWnd *pWaypointLabel1 = GetDlgItem(IDC_WAYPOINTLABEL1_EDIT);
	CWnd *pWaypointLabel2 = GetDlgItem(IDC_WAYPOINTLABEL2_EDIT);
	CWnd *pWaypointLabel3 = GetDlgItem(IDC_WAYPOINTLABEL3_EDIT);

	CWnd *pWaypointLocation = this->GetDlgItem(IDC_WAYPOINT_LOCATION);
	CWnd *pWaypointX = GetDlgItem(IDC_WAYPOINT_LOCATIONX);
	CWnd *pWaypointY = GetDlgItem(IDC_WAYPOINT_LOCATIONY);
	CButton *pBiDirCheck = (CButton *)GetDlgItem(IDC_WAYPOINT_BIDIRECTIONAL);

	if (theTrigger) {
		pCaption1->ShowWindow(SW_SHOW);
		pWnd->ShowWindow(SW_SHOW);
	} else {
		pCaption1->ShowWindow(SW_HIDE);
		pCaption2->ShowWindow(SW_HIDE);
		pWnd->ShowWindow(SW_HIDE);
	}

	if (pCombo && !theTrigger) {
		pCombo->ResetContent();
		pCombo->AddString((TheNameKeyGenerator->keyToName(TheKey_InitialCameraPosition)).str());
		pCombo->AddString((TheNameKeyGenerator->keyToName(TheKey_Player_1_Start)).str());
		pCombo->AddString((TheNameKeyGenerator->keyToName(TheKey_Player_2_Start)).str());
		pCombo->AddString((TheNameKeyGenerator->keyToName(TheKey_Player_3_Start)).str());
		pCombo->AddString((TheNameKeyGenerator->keyToName(TheKey_Player_4_Start)).str());
		pCombo->AddString((TheNameKeyGenerator->keyToName(TheKey_Player_5_Start)).str());
		pCombo->AddString((TheNameKeyGenerator->keyToName(TheKey_Player_6_Start)).str());
		pCombo->AddString((TheNameKeyGenerator->keyToName(TheKey_Player_7_Start)).str());
		pCombo->AddString((TheNameKeyGenerator->keyToName(TheKey_Player_8_Start)).str());
		pCombo->ShowWindow(SW_SHOW);
	}	else if (pCombo && theTrigger) {
		pCombo->ResetContent();
		AsciiString trigger;
		trigger = INNER_PERIMETER;
		trigger.concat("1");
		pCombo->AddString(trigger.str());
		trigger = OUTER_PERIMETER;
		trigger.concat("1");
		pCombo->AddString(trigger.str());
		trigger = INNER_PERIMETER;
		trigger.concat("2");
		pCombo->AddString(trigger.str());
		trigger = OUTER_PERIMETER;
		trigger.concat("2");
		pCombo->AddString(trigger.str());
		trigger = INNER_PERIMETER;
		trigger.concat("3");
		pCombo->AddString(trigger.str());
		trigger = OUTER_PERIMETER;
		trigger.concat("3");
		pCombo->AddString(trigger.str());
		trigger = INNER_PERIMETER;
		trigger.concat("4");
		pCombo->AddString(trigger.str());
		trigger = OUTER_PERIMETER;
		trigger.concat("4");
		pCombo->AddString(trigger.str());
		pCombo->ShowWindow(SW_SHOW);
	}

	// display the list of waypoint names drop down menu
	if (pListWayptNames && !theTrigger) {

		// reset everything and start fresh again
		pListWayptNames->ResetContent();

		// get the first map object, and then cycle through the rest
		MapObject *tempObj = MapObject::getFirstMapObject();
		while (true) {

			if (!tempObj)
				break;

			// if it is a waypoint, add its name to the combo box
			if (tempObj->isWaypoint())
				pListWayptNames->AddString(tempObj->getWaypointName().str());

			tempObj = tempObj->getNext();
		}

		// make sure the window is displayed
		pCombo->ShowWindow(SW_SHOW);
	}

	if ((theMapObj || isWaypointTool) && pWnd ) {
		if (theMapObj) {
			Bool exists;
			pWnd->EnableWindow();
			pWnd->SetWindowText(theMapObj->getProperties()->getAsciiString(TheKey_waypointName).str());
			pCaption1->SetWindowText("Waypoint name:");
			SetWindowText("Waypoint Options");
			CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();

			/* display location of waypoint */

			pWaypointLocation->ShowWindow(SW_SHOW);
			pWaypointY->ShowWindow(SW_SHOW);
			pWaypointX->ShowWindow(SW_SHOW);
			pListWayptNames->ShowWindow(SW_SHOW);
			pCaption5->ShowWindow(SW_SHOW);
			const Coord3D *waypointLocation = getSingleSelectedWaypoint()->getLocation();
			AsciiString locX, locY;

			// convert the location coordinates to strings
			locX.format("%f", waypointLocation->x);
			locY.format("%f", waypointLocation->y);

			// set the window text to reflect the current position of the waypoint
			pWaypointX->SetWindowText(locX.str());
			pWaypointY->SetWindowText(locY.str());

			if (pDoc->isWaypointLinked(theMapObj)) {
				pCaption2->ShowWindow(SW_SHOW);
				AsciiString name;
				name = theMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel1, &exists);
				pWaypointLabel1->ShowWindow(SW_SHOW);
				pWaypointLabel1->SetWindowText(name.str());
				name = theMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel2, &exists);
				pWaypointLabel2->ShowWindow(SW_SHOW);
				pWaypointLabel2->SetWindowText(name.str());
				name = theMapObj->getProperties()->getAsciiString(TheKey_waypointPathLabel3, &exists);
				pWaypointLabel3->ShowWindow(SW_SHOW);
				pWaypointLabel3->SetWindowText(name.str());
			} else {
				pCaption2->ShowWindow(SW_HIDE);
				pWaypointLabel1->ShowWindow(SW_HIDE);
				pWaypointLabel2->ShowWindow(SW_HIDE);
				pWaypointLabel3->ShowWindow(SW_HIDE);
			}
			if (pBiDirCheck) {
				pBiDirCheck->ShowWindow(SW_SHOW);
				Bool checked = theMapObj->getProperties()->getBool(TheKey_waypointPathBiDirectional, &exists);
				pBiDirCheck->SetCheck(checked);
			}
		}
	} else if (theTrigger && pWnd) {
		pListWayptNames->ShowWindow(SW_HIDE);
		pWaypointLocation->ShowWindow(SW_HIDE);
		pWaypointY->ShowWindow(SW_HIDE);
		pWaypointX->ShowWindow(SW_HIDE);
		pCaption3->ShowWindow(SW_HIDE);
		pCaption4->ShowWindow(SW_HIDE);
		pCaption5->ShowWindow(SW_HIDE);
		pWaypointLabel1->ShowWindow(SW_HIDE);
		pWaypointLabel2->ShowWindow(SW_HIDE);
		pWaypointLabel3->ShowWindow(SW_HIDE);
		pCaption2->ShowWindow(SW_HIDE);
		pBiDirCheck->ShowWindow(SW_HIDE);
		pCaption1->SetWindowText("Area name:");
		SetWindowText("Area Trigger Options");
		pWnd->SetWindowText(theTrigger->getTriggerName().str());
		pWnd->EnableWindow();
	}	else if (pWnd) {
		pCaption2->ShowWindow(SW_HIDE);
		pWnd->EnableWindow(false);
		pWnd->SetWindowText("");
	}
}

void WaypointOptions::update()
{
	if (m_staticThis) {
		m_staticThis->updateTheUI();
	}
}

/////////////////////////////////////////////////////////////////////////////
// WaypointOptions message handlers

/// Dialog UI initialization.
/** Creates the slider controls, and sets the initial values for
width and feather in the ui controls. */
BOOL WaypointOptions::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_updating = true;

	m_staticThis = this;
	m_updating = false;
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


BEGIN_MESSAGE_MAP(WaypointOptions, COptionsPanel)
	//{{AFX_MSG_MAP(WaypointOptions)
	ON_CBN_KILLFOCUS(IDC_WAYPOINTNAME_EDIT, OnChangeWaypointnameEdit)
	ON_CBN_KILLFOCUS(IDC_LIST_OF_WAYPOINT_NAMES, OnChangeSelectedWaypoint)
	ON_EN_CHANGE(IDC_WAYPOINT_LOCATIONX, OnEditWaypointLocationX)
	ON_EN_CHANGE(IDC_WAYPOINT_LOCATIONY, OnEditWaypointLocationY)
	ON_EN_CHANGE(IDC_WAYPOINTLABEL1_EDIT, OnEditchangeWaypointlabel1Edit)
	ON_EN_CHANGE(IDC_WAYPOINTLABEL2_EDIT, OnEditchangeWaypointlabel2Edit)
	ON_EN_CHANGE(IDC_WAYPOINTLABEL3_EDIT, OnEditchangeWaypointlabel3Edit)
	ON_CBN_SELENDOK(IDC_LIST_OF_WAYPOINT_NAMES, OnChangeSelectedWaypoint)
	ON_CBN_SELENDOK(IDC_WAYPOINTNAME_EDIT, OnChangeWaypointnameEdit)
	ON_BN_CLICKED(IDC_WAYPOINT_BIDIRECTIONAL, OnWaypointBidirectional)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void WaypointOptions::OnChangeSelectedWaypoint()
{
	// deselect whatever is currently selected
	MapObject *currentlySelected = getSingleSelectedWaypoint(), *pMapObj, *waypt;
	if (!currentlySelected)
		return;
	currentlySelected->setSelected(false);

	// retrieve information from dialog box, if user-typed -- sel will be -1, otherwise it will be >=0
	CString theText;
	CComboBox *pListWayptNames = (CComboBox*)GetDlgItem(IDC_LIST_OF_WAYPOINT_NAMES);
	Int sel = pListWayptNames->GetCurSel();
	if (sel >= 0) {
		pListWayptNames->GetLBText(sel, theText);
	} else {
		pListWayptNames->GetWindowText(theText);
	}
	AsciiString name((LPCTSTR)theText);

	// find and store the waypoint that corresponds to the information in the dialog box
	Bool foundWaypoint = false;
	for (pMapObj = MapObject::getFirstMapObject(); pMapObj; pMapObj = pMapObj->getNext()) {
		if (pMapObj->isWaypoint()) {
			if (pMapObj->getWaypointName() != AsciiString::TheEmptyString) {
				if (pMapObj->getWaypointName() == name) {
					foundWaypoint = true;
					waypt = pMapObj;
					break;
				}
			}
		}
	}

	// if no waypoint corresponds to whatever is in the dialog box, then don't do anything
	if (!foundWaypoint)
		return;

	// select the waypoint and set camera position to position of waypoint
	waypt->setSelected(true);
	Coord3D pos = *waypt->getLocation();
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc) {
		WbView3d *p3View = pDoc->GetActive3DView();
		if (p3View) {
			p3View->setCenterInView(pos.x/MAP_XY_FACTOR, pos.y/MAP_XY_FACTOR);
		}
	}

	update();
}

void WaypointOptions::OnEditWaypointLocationX()
{
	CString tempName;
	AsciiString name;

	//make sure that only one waypoint is being selected currently
	MapObject *waypt = getSingleSelectedWaypoint();
	if (!waypt)
		return;

	// get old waypoint information
	const Coord3D *waypointLocation = waypt->getLocation();
	Coord3D newWaypointLocation;

	// retrieve information from dialog box
	CWnd *pWnd = GetDlgItem(IDC_WAYPOINT_LOCATIONX);
	pWnd->GetWindowText(tempName);
	name = ((LPCTSTR)tempName);

	// make sure that there is a value in the field, otherwise funky stuff would happen
	if (name.isEmpty())
		return;

	// determine new values of waypoint location
	newWaypointLocation.x = atof(name.str());
	newWaypointLocation.y = waypointLocation->y;
	newWaypointLocation.z = 0;

	// set the new information into both the waypointa and the window
	waypt->setLocation(&newWaypointLocation);
}

void WaypointOptions::OnEditWaypointLocationY()
{
	CString tempName;
	AsciiString name;

	//make sure that only one waypoint is being selected currently
	MapObject *waypt = getSingleSelectedWaypoint();
	if (!waypt)
		return;

	// get old waypoint information
	const Coord3D *waypointLocation = waypt->getLocation();
	Coord3D newWaypointLocation;

	// retrieve information from dialog box
	CWnd *pWnd = GetDlgItem(IDC_WAYPOINT_LOCATIONY);
	pWnd->GetWindowText(tempName);
	name = ((LPCTSTR)tempName);

	// make sure that there is a value in the field, otherwise funky stuff would happen
	if (name.isEmpty())
		return;

	// determine new values of waypoint location
	newWaypointLocation.y = atof(name.str());
	newWaypointLocation.x = waypointLocation->x;
	newWaypointLocation.z = 0;

	// set the new information into both the waypointa and the window
	waypt->setLocation(&newWaypointLocation);
}

Bool WaypointOptions::isUnique(AsciiString name, MapObject* theMapObj)
{
	MapObject *pMapObj;
	Bool didMatch = false;
	for (pMapObj = MapObject::getFirstMapObject(); !didMatch && pMapObj; pMapObj = pMapObj->getNext()) {
		if (pMapObj->isWaypoint()) {
			if (pMapObj == theMapObj) continue; // don't check against self.
			AsciiString wayName = pMapObj->getWaypointName();
			if (name == wayName) {
				didMatch = true;
				break;
			}
		}
	}
	return (didMatch == false);
}

AsciiString WaypointOptions::GenerateUniqueName(Int id)
{
	AsciiString name;
	name.format("Waypoint %d", id);
	while (!isUnique(name)) {
		id++;
		name.format("Waypoint %d", id);
	}

	return name;
}

void WaypointOptions::OnChangeWaypointnameEdit()
{
	MapObject *theMapObj = getSingleSelectedWaypoint();
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();

	// get the combo box
	CComboBox *pCombo = (CComboBox*)GetDlgItem(IDC_WAYPOINTNAME_EDIT);
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

		// check waypoint objects.
		didMatch = !isUnique(name, theMapObj);

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
			if (theMapObj) {
				AsciiString layerName = TheLayersList->removeMapObjectFromLayersList(theMapObj);
				theMapObj->setWaypointName(name);
				theMapObj->validate();
				TheLayersList->addMapObjectToLayersList(theMapObj, layerName);
			}	else if (theTrigger) {
				theTrigger->setTriggerName(name);
			}
		}
	}
}


void WaypointOptions::OnEditchangeWaypointlabel1Edit()
{
	changeWaypointLabel(IDC_WAYPOINTLABEL1_EDIT, TheKey_waypointPathLabel1);
}

void WaypointOptions::changeWaypointLabel(Int editControlID, NameKeyType key)
{
	MapObject *theMapObj = getSingleSelectedWaypoint();
	if (theMapObj==nullptr) return;
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc->isWaypointLinked(theMapObj)) {
		return;
	}
	// get the edit box
	CWnd *pWnd = GetDlgItem(editControlID);
	if (pWnd) {
		// get the text
		CString theText;
		pWnd->GetWindowText(theText);
		AsciiString name((LPCTSTR)theText);
		theMapObj->getProperties()->setAsciiString(key, name);
		pDoc->updateLinkedWaypointLabels(theMapObj);
	}
}

void WaypointOptions::OnEditchangeWaypointlabel2Edit()
{
	changeWaypointLabel(IDC_WAYPOINTLABEL2_EDIT, TheKey_waypointPathLabel2);
}

void WaypointOptions::OnEditchangeWaypointlabel3Edit()
{
	changeWaypointLabel(IDC_WAYPOINTLABEL3_EDIT, TheKey_waypointPathLabel3);
}

void WaypointOptions::OnWaypointBidirectional()
{
	MapObject *theMapObj = getSingleSelectedWaypoint();
	if (theMapObj==nullptr) return;
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc->isWaypointLinked(theMapObj)) {
		return;
	}
	// get the edit box
	CButton *pWnd = (CButton *)GetDlgItem(IDC_WAYPOINT_BIDIRECTIONAL);
	if (pWnd) {
		Bool checked = pWnd->GetCheck()==1;
		// get the text
		CString theText;
		pWnd->GetWindowText(theText);
		AsciiString name((LPCTSTR)theText);
		theMapObj->getProperties()->setBool(TheKey_waypointPathBiDirectional, checked);
		pDoc->updateLinkedWaypointLabels(theMapObj);
	}
}
