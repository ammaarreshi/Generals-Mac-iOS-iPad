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

// FILE: HierarchyView.h //////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:    GUIEdit
//
// File name:  HierarchyView.h
//
// Created:    Colin Day, July 2001
//
// Desc:			 Manipulates the window's hierarchy through the tree
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#pragma once

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
#include <windows.h>
#include <commctrl.h>

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "Lib/BaseType.h"
#include "GameClient/GameWindow.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// TYPE DEFINES ///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Add slight improvement to load times -- was 2:30, now 0:02 for test case.
#define USE_FAST_FIND_ITEM 1


// HierarchyOption ------------------------------------------------------------
//-----------------------------------------------------------------------------
typedef enum
{

	HIERARCHY_ADD_AT_TOP,
	HIERARCHY_ADD_AT_BOTTOM,

} HierarchyOption;

// HierarchyView --------------------------------------------------------------
/** This view allows users to manipulate the windows hierarchy using
	* a tree view control */
//-----------------------------------------------------------------------------
class HierarchyView
{

public:

	HierarchyView();
	~HierarchyView();

	void init();
	void reset();
	void shutdown();

	char *getWindowTreeName( GameWindow *window );
	void addWindow( GameWindow *window, HierarchyOption option );  ///< add a window to the view
	void removeWindow( GameWindow *window );  ///< remove window from tree
	void bringWindowToTop( GameWindow *window );  ///< bring window to top of parent list
	void updateWindowName( GameWindow *window );  ///< update tree entry based on name
	void selectWindow( GameWindow *window );  ///< select window

	HWND getTreeHandle();  ///< get the tree control handle
	HWND getHierarchyHandle();  ///< get window handle for the whole dialog

	void setDialogPos( ICoord2D *pos );
	void getDialogPos( ICoord2D *pos );
	void setDialogSize( ICoord2D *size );
	void getDialogSize( ICoord2D *size );

	void setDragWindow( GameWindow *window );
	void setDragTarget( GameWindow *window );
	GameWindow *getDragWindow();
	GameWindow *getDragTarget();
	void moveWindowAheadOf( GameWindow *window, GameWindow *aheadOf );  ///< move hierarchy representation
	void moveWindowChildOf( GameWindow *window, GameWindow *parent );  ///< move hierarchy representation

	Bool validateDragDropOperation( GameWindow *source, GameWindow *target );

	void setPopupTarget( GameWindow *window );  ///< set target for popup menu
	GameWindow *getPopupTarget();  ///< get the popup target window

	HTREEITEM treePointToItem( Int x, Int y );  ///< translate mouse pos to item location
	GameWindow *getWindowFromItem( HTREEITEM treeItem );  ///< get game window from user data in the tree item

protected:

	static LRESULT CALLBACK dialogProc( HWND hWndDialog, UINT message,
																			WPARAM wParam, LPARAM lParam );

	void addWindowToTree( GameWindow *window, HTREEITEM treeParent,
												HierarchyOption option, Bool addChildren,
												Bool addSiblings );  ///< workhorse for addWindow()
	HTREEITEM findItemEntry( HTREEITEM node, GameWindow *window );   ///< workhorse for findTreeEntry
	HTREEITEM findTreeEntry( GameWindow *window );  ///< return entry if in tree

	HWND m_dialog;  ///< window handle for our control dialog
	HWND m_tree;  ///< window handle for the tree control

	GameWindow *m_dragWindow;  ///< for drag drop operations
	GameWindow *m_dragTarget;  ///< target for drag and drop operations while mouse is moving
	GameWindow *m_popupTarget;  ///< the target for right mouse popup menus

#if USE_FAST_FIND_ITEM
	typedef const GameWindow* ConstGameWindowPtr;
 	// use special class for hashing, since std::hash won't compile for arbitrary ptrs
 	struct hashConstGameWindowPtr
 	{
 		size_t operator()(ConstGameWindowPtr p) const
 		{
 			std::hash<UnsignedInt> hasher;
 			return hasher((UnsignedInt)p);
 		}
 	};

	typedef std::hash_map< ConstGameWindowPtr, HTREEITEM, hashConstGameWindowPtr, std::equal_to<ConstGameWindowPtr>/**/> TreeHash;

 	TreeHash 		m_treeHash;	///< Speed up the search with a nice hash.
#endif


};

///////////////////////////////////////////////////////////////////////////////
// INLINING ///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline HWND HierarchyView::getTreeHandle() { return m_tree; }
inline HWND HierarchyView::getHierarchyHandle() { return m_dialog; }
inline void HierarchyView::setDragWindow( GameWindow *window ) { m_dragWindow = window; }
inline void HierarchyView::setDragTarget( GameWindow *window ) { m_dragTarget = window; }
inline GameWindow *HierarchyView::getDragWindow() { return m_dragWindow; }
inline GameWindow *HierarchyView::getDragTarget() { return m_dragTarget; }
inline void HierarchyView::setPopupTarget( GameWindow *window ) { m_popupTarget = window; }
inline GameWindow *HierarchyView::getPopupTarget() { return m_popupTarget; }

// EXTERNALS //////////////////////////////////////////////////////////////////
extern HierarchyView *TheHierarchyView;  ///< singleton for our view
