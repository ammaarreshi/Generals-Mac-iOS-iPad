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

// RulerTool.cpp
// Author: Mike Lytle, January 2003


#include "StdAfx.h"
#include "resource.h"

#include "RulerTool.h"
#include "MainFrm.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "wbview3d.h"
#include "ObjectTool.h"


// Saved off so that static functions can access its members.
RulerTool*	RulerTool::m_staticThis = nullptr;

/// Constructor
RulerTool::RulerTool() :
Tool(ID_RULER_TOOL, IDC_POINTER)
{
	m_downPt3d.set(0.0f, 0.0f, 0.0f);
	m_rulerType = RULER_LINE;
	m_View = nullptr;
	m_staticThis = this;
}

/// Destructor
RulerTool::~RulerTool()
{
}

// Activate.
void RulerTool::activate()
{
	Tool::activate();
	CMainFrame::GetMainFrame()->showOptionsDialog(IDD_RULER_OPTIONS);
	if (m_View != nullptr) {
		// Is it dangerous to assume that the pointer is still good?
		m_View->doRulerFeedback(m_rulerType);
	}
}

// Deactivate.
void RulerTool::deactivate()
{
	Tool::deactivate();

	if (m_View != nullptr) {
		m_View->doRulerFeedback(RULER_NONE);
	}

}

/** Set the cursor. */
void RulerTool::setCursor()
{
	Tool::setCursor();
}


/** Execute the tool on mouse down */
void RulerTool::mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (m != TRACK_L) return;

	if (m_View == nullptr) {
		// Save so that when we are done the view can stop drawing the rulers.
		m_View = pView;
	}

	Coord3D cpt;
	pView->viewToDocCoords(viewPt, &cpt);

	m_downPt3d = cpt;
	pView->snapPoint(&m_downPt3d);
}

/// Left button move code.
void RulerTool::mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (m != TRACK_L) return;

	if (m_View == nullptr) {
		// Save so that when we are done the view can stop drawing the rulers.
		m_View = pView;
	}

	Coord3D cpt;
	pView->viewToDocCoords(viewPt, &cpt, false);

	if (m_rulerType == RULER_CIRCLE) {
		Coord3D pt;
		pt.x = cpt.x + m_savedLength;
		pt.y = cpt.y + m_savedLength;
		pt.z = cpt.z;
		RulerOptions::setWidth(m_savedLength);
		pView->doRulerFeedback(RULER_CIRCLE);
		pView->rulerFeedbackInfo(cpt, pt, m_savedLength);
		pView->Invalidate();
	} else { //m_rulerType == RULER_LINE
		Coord3D diff;
		diff.set(cpt);
		diff.sub(m_downPt3d);
		m_savedLength = diff.length();
		RulerOptions::setWidth(m_savedLength);
		pView->doRulerFeedback(RULER_LINE);
		pView->rulerFeedbackInfo(cpt, m_downPt3d, m_savedLength);
		pView->Invalidate();
	}

	pDoc->updateAllViews();
}

void RulerTool::setLength(Real length)
{
	if (!m_staticThis || (m_staticThis->m_rulerType != RULER_CIRCLE)) {
		// This should only be called when the ruler type is a circle.
		// Line rulers are always extended by the mouse.
		return;
	}

	m_staticThis->m_savedLength = length;
	if (m_staticThis->m_View) {
		m_staticThis->m_View->rulerFeedbackInfo(m_staticThis->m_downPt3d, m_staticThis->m_downPt3d, length);
		m_staticThis->m_View->Invalidate();
	}

	CString str;
 	str.Format("Diameter (in feet): %f", length * 2.0f);
	CMainFrame::GetMainFrame()->SetMessageText(str);
}

Bool RulerTool::switchType()
{
	if (!m_staticThis) {
		return (FALSE);
	}

	if (m_staticThis->m_rulerType == RULER_LINE) {
		m_staticThis->m_rulerType = RULER_CIRCLE;
	} else {
		m_staticThis->m_rulerType = RULER_LINE;
	}
	if (m_staticThis->m_View != nullptr) {
		m_staticThis->m_View->doRulerFeedback(m_staticThis->m_rulerType);
	}

	return (TRUE);
}

int	RulerTool::getType()
{
	if (!m_staticThis) {
		return (RULER_NONE);
	}

	return (m_staticThis->m_rulerType);
}

Real RulerTool::getLength()
{
	if (m_staticThis) {
		return m_staticThis->m_savedLength;
	}

	return (0.0f);
}
