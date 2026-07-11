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

// FILE: LayersList.cpp
/*---------------------------------------------------------------------------*/
/* EA Pacific                                                                */
/* Confidential Information	                                                 */
/* Copyright (C) 2001 - All Rights Reserved                                  */
/* DO NOT DISTRIBUTE                                                         */
/*---------------------------------------------------------------------------*/
/* Project:    RTS3                                                          */
/* File name:  LayersList.cpp                                                */
/* Created:    John K. McDonald, Jr., 5/10/2002                              */
/* Desc:       The layers list implementation for WorldBuilder.              */
/* Revision History:                                                         */
/*		5/10/2002 : Initial creation                                           */
/*---------------------------------------------------------------------------*/

#include "StdAfx.h"
#include "Common/STLTypedefs.h"
#include "resource.h"
#include "LayersList.h"

#include "Common/Dict.h"
#include "Common/MapObject.h"
#include "Common/WellKnownKeys.h"

#include "PointerTool.h"
#include "WorldBuilderDoc.h"
#include "GameLogic/PolygonTrigger.h"

#include <stack>


static int newLayerNum = 1;

// CLLTreeCtrl Implementation /////////////////////////////////////////////////////////////////////

void CLLTreeCtrl::buildMoveMenu(CMenu* moveMenu, UINT firstID)
{
	LayersList* pParent = (LayersList*) GetParent();
	if (!pParent) {
		return;
	}

	// to save the copy
	const ListLayer& layer = pParent->GetAllLayers();

	ListLayer::const_iterator cit;

	int i = 0;
	for (cit = layer.begin(); cit != layer.end(); ++cit) {
		moveMenu->AppendMenu(MF_STRING, firstID + i, cit->layerName.str());
		++i;
	}
}

enum LayersListMenuSelectionsEnum {
	LAYERS_LIST_SELECT_OBJECT,
	LAYERS_LIST_SELECT_ACTIVE_LAYER,
	LAYERS_LIST_BREAK_1,
	LAYERS_LIST_INSERT_NEW_LAYER,
	LAYERS_LIST_DELETE_CURRENT_LAYER,
	LAYERS_LIST_MERGE_LAYER_INTO,
	LAYERS_LIST_MERGE_OBJECT_INTO,
	LAYERS_LIST_MERGE_VIEW_SELECTION_INTO,
	LAYERS_LIST_BREAK_2,
	LAYERS_LIST_HIDE_CURRENT_LAYER,
};
void CLLTreeCtrl::OnRButtonDown(UINT nFlags, CPoint point)
{
	// first, if there's something under the mouse, select it.

	HTREEITEM item = HitTest(point);
	Bool contextIsLayer = false;
	SelectItem(item);

	if (item) {
		if (GetParentItem(item) == nullptr) {
			mLastClickedLayer = GetItemText(item);
			mLastClickedObject = AsciiString::TheEmptyString;
			contextIsLayer = true;
		} else {
			mLastClickedLayer = AsciiString::TheEmptyString;
			mLastClickedObject = GetItemText(item);
		}
	} else {
		mLastClickedLayer = AsciiString::TheEmptyString;
		mLastClickedObject = AsciiString::TheEmptyString;
	}

	CMenu menu;
	VERIFY(menu.LoadMenu(IDR_LAYERSLISTPOPUP));
	CMenu* pPopup = menu.GetSubMenu(0);
	if (!pPopup) {
		return;
	}
	ClientToScreen(&point);

	CMenu *moveViewSelectionMenu = pPopup->GetSubMenu(LAYERS_LIST_MERGE_VIEW_SELECTION_INTO);
	moveViewSelectionMenu->RemoveMenu(0, MF_BYPOSITION);
	buildMoveMenu(moveViewSelectionMenu, ID_LAYERSLIST_MERGEVIEWSELECTIONINTO_BEGIN);

	if (item) {
		if (contextIsLayer) {
			CString itemText = GetItemText(item);
			if ((itemText.CompareNoCase(LayersList::TheActiveLayerName.c_str()) == 0) ||
				((LayersList::TheActiveLayerName.c_str() == nullptr) &&
				(LayersList::TheUnmutableDefaultLayerName.compare(LayersList::TheDefaultLayerName) == 0))) {
				// Don't allow the current layer to be hidden.
				// Because the objects will immediately disappear when placed.
				pPopup->EnableMenuItem(ID_HIDECURRENTLAYER, MF_GRAYED);
			} else {
				LayersList *ll = (LayersList*) GetParent();
				Bool hidden = ll->isLayerHidden(AsciiString(itemText));
				pPopup->CheckMenuItem(ID_HIDECURRENTLAYER, MF_BYCOMMAND | (hidden ? MF_CHECKED : MF_UNCHECKED));
			}

			CMenu *moveMenu = pPopup->GetSubMenu(LAYERS_LIST_MERGE_LAYER_INTO);
			moveMenu->RemoveMenu(0, MF_BYPOSITION);
			buildMoveMenu(moveMenu, ID_LAYERSLIST_MERGELAYERINTO_BEGIN);

			pPopup->RemoveMenu(LAYERS_LIST_MERGE_VIEW_SELECTION_INTO, MF_BYPOSITION);
			pPopup->RemoveMenu(LAYERS_LIST_MERGE_OBJECT_INTO, MF_BYPOSITION);
			pPopup->RemoveMenu(LAYERS_LIST_SELECT_OBJECT, MF_BYPOSITION);

		} else {
			pPopup->EnableMenuItem(ID_DELETECURRENTLAYER, MF_GRAYED);
			pPopup->EnableMenuItem(ID_HIDECURRENTLAYER, MF_GRAYED);

			CMenu* moveMenu = pPopup->GetSubMenu(LAYERS_LIST_MERGE_OBJECT_INTO);
			moveMenu->RemoveMenu(0, MF_BYPOSITION);
			buildMoveMenu(moveMenu, ID_LAYERSLIST_MERGEOBJECTINTO_BEGIN);

			pPopup->RemoveMenu(LAYERS_LIST_MERGE_LAYER_INTO, MF_BYPOSITION);
		}
	} else {
		pPopup->RemoveMenu(LAYERS_LIST_HIDE_CURRENT_LAYER, MF_BYPOSITION);
		pPopup->RemoveMenu(LAYERS_LIST_MERGE_OBJECT_INTO, MF_BYPOSITION);
		pPopup->RemoveMenu(LAYERS_LIST_MERGE_LAYER_INTO, MF_BYPOSITION);
		pPopup->RemoveMenu(LAYERS_LIST_DELETE_CURRENT_LAYER, MF_BYPOSITION);
		pPopup->RemoveMenu(LAYERS_LIST_SELECT_ACTIVE_LAYER, MF_BYPOSITION);
		pPopup->RemoveMenu(LAYERS_LIST_SELECT_OBJECT, MF_BYPOSITION);
	}

	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, GetParent());
}

BEGIN_MESSAGE_MAP(CLLTreeCtrl, CTreeCtrl)
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

// LayersList Implementation //////////////////////////////////////////////////////////////////////

LayersList::LayersList(UINT nIDTemplate, CWnd *parentWnd) : CDialog(nIDTemplate, parentWnd)
{
	mTree = nullptr;
	m_activatedLayer = false;
	resetLayers();
}

LayersList::~LayersList()
{
	// die die die
	// free up windows resources.
	DestroyWindow();
	delete mTree;
}

void LayersList::resetLayers()
{
	// @todo Default needs to be a localizable string
	Layer defaultLayer;
	TheDefaultLayerName = TheUnmutableDefaultLayerName;
	defaultLayer.layerName = AsciiString(TheDefaultLayerName.c_str());
	defaultLayer.show = true;
	mLayers.clear();
	mLayers.push_back(defaultLayer);
	defaultLayer.layerName = AsciiString(ThePolygonTriggerLayerName.c_str());
	mLayers.push_back(defaultLayer);
	TheLayersList = this;
	if (mTree) {
		mTree->DeleteAllItems();
		updateUIFromList();
	}
}

void LayersList::addMapObjectToLayersList(MapObject *objToAdd, AsciiString layerToAddTo)
{
	if (!objToAdd || findMapObjectAndList(objToAdd)) {
		DEBUG_CRASH(("MapObject added was null or object already in Layers List. jkmcd"));
		return;
	}
	ListLayerIt layerIt;

	if (!findLayerNamed(layerToAddTo, &layerIt)) {
		// try to add a layer of that name.
		addLayerNamed(layerToAddTo);
		if (!findLayerNamed(layerToAddTo, &layerIt)) {
			// Try to place it in the active layer.
			if (!findLayerNamed(AsciiString(TheActiveLayerName.c_str()), &layerIt)) {
				// if we still can't find that layer name, then try adding it to the default layer.
				if (!findLayerNamed(AsciiString(TheDefaultLayerName.c_str()), &layerIt)) {
					return;
				}
			}
		}
	}

	addMapObjectToLayer(objToAdd, &layerIt);
}

void LayersList::addPolygonTriggerToLayersList(PolygonTrigger *triggerToAdd, AsciiString layerToAddTo)
{
	if (!triggerToAdd || findPolygonTriggerAndList(triggerToAdd)) {
		DEBUG_CRASH(("PolygonTrigger added was null or object already in Layers List. jkmcd"));
		return;
	}
	ListLayerIt layerIt;

	if (!findLayerNamed(layerToAddTo, &layerIt)) {
		// try to add a layer of that name.
		addLayerNamed(layerToAddTo);
		if (!findLayerNamed(layerToAddTo, &layerIt)) {
			// Try to place it in the active layer.
			if (!findLayerNamed(AsciiString(TheActiveLayerName.c_str()), &layerIt)) {
				// if we still can't find that layer name, then try adding it to the trigger layer.
				if (!findLayerNamed(AsciiString(ThePolygonTriggerLayerName.c_str()), &layerIt)) {
					return;
				}
			}
		}
	}

	addPolygonTriggerToLayer(triggerToAdd, &layerIt);
}


AsciiString LayersList::removeMapObjectFromLayersList(MapObject *objToRemove)
{
	ListLayerIt layerIt;
	ListMapObjectPtrIt objIt;
	if (!objToRemove || !findMapObjectAndList(objToRemove, &layerIt, &objIt)) {
		DEBUG_CRASH(("Couldn't find that object in the layers list. jkmcd"));
		return AsciiString::TheEmptyString;
	}

	removeMapObjectFromLayer(objToRemove, &layerIt, &objIt);
	return layerIt->layerName;
}

AsciiString LayersList::removePolygonTriggerFromLayersList(PolygonTrigger *triggerToRemove)
{
	ListLayerIt layerIt;
	ListPolygonTriggerPtrIt triggerIt;
	if (!triggerToRemove || !findPolygonTriggerAndList(triggerToRemove, &layerIt, &triggerIt)) {
		DEBUG_CRASH(("Couldn't find that polygon trigger in the layers list. jkmcd"));
		return AsciiString::TheEmptyString;
	}

	removePolygonTriggerFromLayer(triggerToRemove, &layerIt, &triggerIt);
	return layerIt->layerName;
}


void LayersList::changeMapObjectLayer(MapObject *objToChange, AsciiString layerToPlaceOn)
{
	if (!objToChange) {
		DEBUG_CRASH(("Attempted to change location of null object. jkmcd"));
		return;
	}

	removeMapObjectFromLayersList(objToChange);
	addMapObjectToLayersList(objToChange, layerToPlaceOn);
}

void LayersList::changePolygonTriggerLayer(PolygonTrigger *triggerToChange, AsciiString layerToPlaceOn)
{
	if (!triggerToChange) {
		DEBUG_CRASH(("Attempted to change location of null object. jkmcd"));
		return;
	}

	removePolygonTriggerFromLayersList(triggerToChange);
	addPolygonTriggerToLayersList(triggerToChange, layerToPlaceOn);
}

void LayersList::addLayerNamed(IN AsciiString layerToAdd)
{
	if (findLayerNamed(layerToAdd)) {
		DEBUG_CRASH(("Already found a layer named %s", layerToAdd.str()));
		return;
	}

	if (layerToAdd.isEmpty()) {
		return;
	}

	Layer newLayer;
	newLayer.layerName = layerToAdd;
	newLayer.show = true;
	mLayers.push_back(newLayer);
}

void LayersList::removeLayerNamed(IN AsciiString layerToRemove)
{
	ListLayerIt layerIt;
	// Not allowed to remove the Default layer
	if (layerToRemove.compareNoCase(TheDefaultLayerName.c_str()) == 0) {
		return;
	}

	// Not allowed to remove the Default trigger layer
	if (layerToRemove.compareNoCase(ThePolygonTriggerLayerName.c_str()) == 0) {
		return;
	}


	// If we can't find the layer, how can we remove it?
	if (!findLayerNamed(layerToRemove, &layerIt)) {
		DEBUG_CRASH(("Couldn't find layer named %s", layerToRemove.str()));
		return;
	}

	// Transfer all the objects from the old layer to the default layer.
	for (ListMapObjectPtrIt obj = layerIt->objectsInLayer.begin(); obj != layerIt->objectsInLayer.end(); obj = layerIt->objectsInLayer.begin()) {
		changeMapObjectLayer((*obj), AsciiString(TheDefaultLayerName.c_str()));
	}

	// Finally, remove the layer itself.
	mLayers.erase(layerIt);
}

void LayersList::changeLayerName(IN AsciiString oldLayerName, AsciiString newLayerName)
{
	ListLayerIt layerIt;
	if (!findLayerNamed(oldLayerName, &layerIt)) {
		DEBUG_CRASH(("Couldn't find the layer named %s", oldLayerName.str()));
		return;
	}

	layerIt->layerName = newLayerName;

	if (oldLayerName.compareNoCase(TheDefaultLayerName.c_str())) {
		TheDefaultLayerName = newLayerName.str();
	}
}

void LayersList::mergeLayerInto(IN ListLayerIt src, IN ListLayerIt dst)
{
	CWorldBuilderDoc* pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) {
		return;
	}

	ListMapObjectPtrIt mopIt;
	for (mopIt = src->objectsInLayer.begin(); mopIt != src->objectsInLayer.end(); mopIt = src->objectsInLayer.begin()) {
		changeMapObjectLayer(*mopIt, dst->layerName);
	}

	ListPolygonTriggerPtrIt ptpIt;
	for (ptpIt = src->polygonTriggersInLayer.begin(); ptpIt != src->polygonTriggersInLayer.end(); ptpIt = src->polygonTriggersInLayer.begin()) {
		changePolygonTriggerLayer(*ptpIt, dst->layerName);
	}


	// Don't remove these layers.
	if (src->layerName.compareNoCase(TheDefaultLayerName.c_str()) == 0) {
		return;
	}
	if (src->layerName.compareNoCase(ThePolygonTriggerLayerName.c_str()) == 0) {
		return;
	}

	if (src->layerName.compareNoCase(TheActiveLayerName.c_str()) == 0) {
		// The active layer is no longer available.
		TheActiveLayerName = AsciiString::TheEmptyString.str();
	}


	// remove the layer from the tree view
	HTREEITEM itemToRemove = findTreeLayerNamed(src->layerName);
	if (!itemToRemove) {
		return;
	}

	mTree->DeleteItem(itemToRemove);

	mLayers.erase(src);
}

Bool LayersList::isLayerHidden(IN AsciiString layerToTest)
{
	ListLayerIt layerIt;
	if (!findLayerNamed(layerToTest, &layerIt)) {
		// those that we cannot find are always visible. (Not really, but pick one)
		return false;
	}

	// !show == hide
	return (!layerIt->show);
}

void LayersList::updateUIFromList()
{
	if (!m_performUpdates) {
		return;
	}
	CTreeCtrl *pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);

	if (!pTree) {
		return;
	}

	pTree->DeleteAllItems();

//	int index = 0;

	for (ListLayerIt layersIt = mLayers.begin(); layersIt != mLayers.end(); ++layersIt) {
		// Add a branch for each Layer.
		int iconToShow = (layersIt->show ? 0 : 1);
		HTREEITEM thisBranch = pTree->InsertItem(layersIt->layerName.str(), iconToShow, iconToShow);

		for (ListMapObjectPtrIt objIt = layersIt->objectsInLayer.begin(); objIt != layersIt->objectsInLayer.end(); ++objIt) {
			Bool exists;
			AsciiString uniqueID = (*objIt)->getProperties()->getAsciiString(TheKey_uniqueID, &exists);
			if (exists) {
				pTree->InsertItem(uniqueID.str(), iconToShow, iconToShow, thisBranch);
			} else {
				pTree->InsertItem((*objIt)->getName().str(), iconToShow, iconToShow, thisBranch);
			}
		}

		for (ListPolygonTriggerPtrIt triggerIt = layersIt->polygonTriggersInLayer.begin(); triggerIt != layersIt->polygonTriggersInLayer.end(); ++triggerIt) {
			const AsciiString& uniqueID = (*triggerIt)->getTriggerName();
			pTree->InsertItem(uniqueID.str(), iconToShow, iconToShow, thisBranch);
		}
	}
}

Bool LayersList::findMapObjectAndList(IN MapObject *objectToFind, OUT ListLayerIt *layerIt, OUT ListMapObjectPtrIt *objectIt)
{
	if (!objectToFind) {
		return false;
	}

	for (ListLayerIt layersIt = mLayers.begin(); layersIt != mLayers.end(); ++layersIt) {
		for (ListMapObjectPtrIt objPtrIt = layersIt->objectsInLayer.begin(); objPtrIt != layersIt->objectsInLayer.end(); ++objPtrIt) {
			if ((*objPtrIt) == objectToFind) {
				if (layerIt) {
					(*layerIt) = layersIt;
				}

				if (objectIt) {
					(*objectIt) = objPtrIt;
				}

				return true;
			}
		}
	}

	// if we get here, it means we didn't find the object anywhere in any of the layers
	return false;
}

Bool LayersList::findPolygonTriggerAndList(IN PolygonTrigger *triggerToFind, OUT ListLayerIt *layerIt, OUT ListPolygonTriggerPtrIt *triggerIt)
{
	if (!triggerToFind) {
		return false;
	}

	for (ListLayerIt layersIt = mLayers.begin(); layersIt != mLayers.end(); ++layersIt) {
		for (ListPolygonTriggerPtrIt triggerPtrIt = layersIt->polygonTriggersInLayer.begin(); triggerPtrIt != layersIt->polygonTriggersInLayer.end(); ++triggerPtrIt) {
			if ((*triggerPtrIt) == triggerToFind) {
				if (layerIt) {
					(*layerIt) = layersIt;
				}

				if (triggerIt) {
					(*triggerIt) = triggerPtrIt;
				}

				return true;
			}
		}
	}

	// if we get here, it means we didn't find the object anywhere in any of the layers
	return false;
}


Bool LayersList::findLayerNamed(IN AsciiString layerName, OUT ListLayerIt *layerIt)
{
	if (layerName.isEmpty()) {
		return false;
	}

	for (ListLayerIt layersIt = mLayers.begin(); layersIt != mLayers.end(); ++layersIt) {
		if (layersIt->layerName.compareNoCase(layerName) == 0) {
			if (layerIt) {
				(*layerIt) = layersIt;
			}

			return true;
		}
	}

	// if we get here, it means we didn't find the named layer
	return false;
}

void LayersList::addMapObjectToLayer(IN MapObject *objectToAdd, IN ListLayerIt *layerIt)
{
	if (!objectToAdd) {
		DEBUG_CRASH(("No object to add in addMapObjectToLayer. This should not happen. jkmcd"));
		return;
	}

	ListLayerIt layerToAddTo;
	if (layerIt) {
		layerToAddTo = *layerIt;
	} else {
		// By default, add to the default layer.
		if (!findLayerNamed(AsciiString(TheDefaultLayerName.c_str()), &layerToAddTo)) {
			// If we couldn't find that layer, then we're fuxored.
			return;
		}
	}

	Dict *objProps = objectToAdd->getProperties();

	if (layerToAddTo->layerName.compareNoCase(AsciiString(TheDefaultLayerName.c_str())) == 0) {
		objProps->setAsciiString(TheKey_objectLayer, AsciiString::TheEmptyString);
	} else {
		objProps->setAsciiString(TheKey_objectLayer, layerToAddTo->layerName);
	}

	// layerToAddTo now has a valid layer to add to.
	layerToAddTo->objectsInLayer.push_back(objectToAdd);

	// only update this object.
	HTREEITEM hItem = findTreeLayerNamed(layerToAddTo->layerName);
	if (hItem) {
		Bool dontcare;
		AsciiString objName = objProps->getAsciiString(TheKey_uniqueID, &dontcare);
		int iconToShow = (layerToAddTo->show ? 0 : 1);
		mTree->InsertItem(objName.str(), iconToShow, iconToShow, hItem);
	}

	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) {
		return;
	}

	if (!layerToAddTo->show) {
		objectToAdd->setFlag(FLAG_DONT_RENDER);
	} else {
		objectToAdd->clearFlag(FLAG_DONT_RENDER);
	}
	pDoc->invalObject(objectToAdd);
	//PointerTool::clearSelection();

}

void LayersList::addPolygonTriggerToLayer(IN PolygonTrigger *triggerToAdd, IN ListLayerIt *layerIt)
{
	if (!triggerToAdd) {
		DEBUG_CRASH(("No trigger to add in addPolygonTriggerToLayer. This should not happen. jkmcd"));
		return;
	}

	ListLayerIt layerToAddTo;
	if (layerIt) {
		layerToAddTo = *layerIt;
	} else {
		// By default, add to the default layer.
		if (!findLayerNamed(AsciiString(ThePolygonTriggerLayerName.c_str()), &layerToAddTo)) {
			// If we couldn't find that layer, then we're fuxored.
			return;
		}
	}

	if (layerToAddTo->layerName.compareNoCase(AsciiString(ThePolygonTriggerLayerName.c_str())) == 0) {
		triggerToAdd->setLayerName(AsciiString::TheEmptyString);
	} else {
		triggerToAdd->setLayerName(layerToAddTo->layerName);
	}

	// layerToAddTo now has a valid layer to add to.
	layerToAddTo->polygonTriggersInLayer.push_back(triggerToAdd);

	// only update this object.
	HTREEITEM hItem = findTreeLayerNamed(layerToAddTo->layerName);
	if (hItem) {
		const AsciiString& triggerName = triggerToAdd->getTriggerName();
		int iconToShow = (layerToAddTo->show ? 0 : 1);
		mTree->InsertItem(triggerName.str(), iconToShow, iconToShow, hItem);
	}

	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) {
		return;
	}

	if (!layerToAddTo->show) {
		triggerToAdd->setShouldRender(false);
	} else {
		triggerToAdd->setShouldRender(true);
	}

}


void LayersList::removeMapObjectFromLayer(IN MapObject *objectToRemove, IN ListLayerIt *layerIt, IN ListMapObjectPtrIt *objectIt)
{
	if (!objectToRemove) {
		DEBUG_CRASH(("Attempted to remove null object from layers list. jkmcd"));
		return;
	}

	ListLayerIt layerToRemoveFrom;
	ListMapObjectPtrIt objectBeingRemove;

	if (layerIt && objectIt) {
		layerToRemoveFrom = (*layerIt);
		objectBeingRemove = (*objectIt);
	} else {
		if (!findMapObjectAndList(objectToRemove, &layerToRemoveFrom, &objectBeingRemove)) {
			DEBUG_CRASH(("Couldn't find the object anywhere. Why did we try to remove it? jkmcd"));
			return;
		}
	}

	// only remove this object
	HTREEITEM layer = findTreeLayerNamed(layerToRemoveFrom->layerName);
	if (layer) {
		Bool dontcare;
		AsciiString objectUID = (*objectBeingRemove)->getProperties()->getAsciiString(TheKey_uniqueID, &dontcare);
		HTREEITEM itemToDelete = findTreeObjectNamed(objectUID.str(), layer);
		if (itemToDelete) {
			mTree->DeleteItem(itemToDelete);
		}
	}

	// don't need to update the layer in the object because only 1 of 2 things can be happening:
	// 1. We are about to delete the object
	// 2. The object is about to be moved to the default layer (and will be updated anyways.)

	// layerToRemoveFrom and objectToRemove are now valid. Remove em.
	layerToRemoveFrom->objectsInLayer.erase(objectBeingRemove);

}

void LayersList::removePolygonTriggerFromLayer(IN PolygonTrigger *triggerToRemove, IN ListLayerIt *layerIt, IN ListPolygonTriggerPtrIt *triggerIt)
{
	if (!triggerToRemove) {
		DEBUG_CRASH(("Attempted to remove null trigger from layers list. jkmcd"));
		return;
	}

	ListLayerIt layerToRemoveFrom;
	ListPolygonTriggerPtrIt triggerBeingRemove;

	if (layerIt && triggerIt) {
		layerToRemoveFrom = (*layerIt);
		triggerBeingRemove = (*triggerIt);
	} else {
		if (!findPolygonTriggerAndList(triggerToRemove, &layerToRemoveFrom, &triggerBeingRemove)) {
			DEBUG_CRASH(("Couldn't find the object anywhere. Why did we try to remove it? jkmcd"));
			return;
		}
	}

	// only remove this object
	HTREEITEM layer = findTreeLayerNamed(layerToRemoveFrom->layerName);
	if (layer) {
		const AsciiString& triggerUID = (*triggerBeingRemove)->getTriggerName();
		HTREEITEM itemToDelete = findTreeObjectNamed(triggerUID.str(), layer);
		if (itemToDelete) {
			mTree->DeleteItem(itemToDelete);
		}
	}

	// don't need to update the layer in the object because only 1 of 2 things can be happening:
	// 1. We are about to delete the object
	// 2. The object is about to be moved to the default layer (and will be updated anyways.)

	// layerToRemoveFrom and objectToRemove are now valid. Remove em.
	layerToRemoveFrom->polygonTriggersInLayer.erase(triggerBeingRemove);

}


BOOL LayersList::OnInitDialog()
{
	// call the parent first
	CDialog::OnInitDialog();

	// Set up the initial tree.
	CTreeCtrl *pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);

	if (!pTree) {
		return 1;
	}

	CRect rect;
	mTree = new CLLTreeCtrl;
	pTree->GetWindowRect(&rect);
	ScreenToClient(&rect);

	DWORD style = pTree->GetStyle();
	mTree->Create(style, rect, this, IDC_LL_TREE);
	pTree->DestroyWindow();

	pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);
	// pTree should == mTree now.

	mImageList.Create(16, 16, ILC_COLOR8, 3, 3);

	// If the order of these are changed, please update the enum in updateTreeImages.
	mImageList.Add(LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_Show)));
	mImageList.Add(LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_Hide)));
	mImageList.Add(LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_Selected)));

	pTree->SetImageList(&mImageList, LVSIL_NORMAL);
	pTree->InsertItem(TheDefaultLayerName.c_str(), 0, 0);


	return 1;

}

// returning 0 means editing can continue.
// returning 1 means editing is cancelled
void LayersList::OnBeginEditLabel(NMHDR *pNotifyStruct, LRESULT* pResult)
{
	CTreeCtrl *pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);
	if (!pTree) {
		(*pResult) = 1;
		return;
	}

	TV_DISPINFO *ptvdi = (TV_DISPINFO*) pNotifyStruct;
	if (ptvdi == nullptr) {
		(*pResult) = 1;
		return;
	}

	if (!ptvdi->item.pszText) {
		(*pResult) = 1;
		return;
	}

	if (strcmp(ptvdi->item.pszText, ThePolygonTriggerLayerName.c_str()) == 0) {
		// Can't edit the default polygon trigger layer name.
		(*pResult) = 1;
		return;
	}


	CString str = pTree->GetItemText(ptvdi->item.hItem);
	// if we can't find the layer of that name, then
	if (!findLayerNamed(AsciiString(str))) {
		// End the editing
		(*pResult) = 1;
		return;
	}

	mCurrentlyEditingLabel = AsciiString(str);
	(*pResult) = 0;
}

void LayersList::OnEndEditLabel(NMHDR *pNotifyStruct, LRESULT* pResult)
{
	CTreeCtrl *pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);
	if (!pTree) {
		return;
	}

	TV_DISPINFO *ptvdi = (TV_DISPINFO*) pNotifyStruct;
	if (!ptvdi->item.pszText) {
		return;
	}

	ListLayerIt layerIt;
	if (!findLayerNamed(mCurrentlyEditingLabel, &layerIt)) {
		return;
	}

	layerIt->layerName = AsciiString(ptvdi->item.pszText);
	if (mCurrentlyEditingLabel.compareNoCase(AsciiString(TheDefaultLayerName.c_str())) == 0) {
		// update the default label to be the new layer name.
		TheDefaultLayerName = layerIt->layerName.str();
	}

	// do this to prevent rebuilding the tree.
	pTree->SetItemText(ptvdi->item.hItem, layerIt->layerName.str());

	mCurrentlyEditingLabel = AsciiString::TheEmptyString;
}


void LayersList::OnNewLayer()
{
	CTreeCtrl *pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);
	if (!pTree) {
		return;
	}

	static char buffer[1024];
	snprintf(buffer, ARRAY_SIZE(buffer), "%s %d", TheDefaultNewLayerName.c_str(), newLayerNum);
	addLayerNamed(buffer);

	HTREEITEM newItem = pTree->InsertItem(buffer, 0, 0);
	pTree->SelectItem(newItem);
	pTree->EditLabel(newItem);

	++newLayerNum;
}

void LayersList::OnDeleteLayer()
{
	CTreeCtrl *pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);
	if (!pTree) {
		return;
	}

	HTREEITEM item = pTree->GetSelectedItem();
	if (!item) {
		return;
	}

	CString catToDelete = pTree->GetItemText(item);
	AsciiString asciiCatToDelete(catToDelete.GetBuffer(0));

	if (asciiCatToDelete.compareNoCase(AsciiString(TheDefaultLayerName.c_str())) == 0) {
		return;
	}

	if (asciiCatToDelete.compareNoCase(AsciiString(ThePolygonTriggerLayerName.c_str())) == 0) {
		return;
	}

	ListLayerIt srcLayerIt, dstLayerIt;
	if (!findLayerNamed(asciiCatToDelete, &srcLayerIt)) {
		return;
	}


	if (asciiCatToDelete.compareNoCase(TheActiveLayerName.c_str()) == 0) {
		// The active layer is being deleted.
		TheActiveLayerName = AsciiString::TheEmptyString.str();
		updateTreeImages();

		if (!findLayerNamed(AsciiString(TheDefaultLayerName.c_str()), &dstLayerIt)) {
			return;
		}
	} else {

		if (!findLayerNamed(AsciiString(TheActiveLayerName.c_str()), &dstLayerIt)) {
			if (!findLayerNamed(AsciiString(TheDefaultLayerName.c_str()), &dstLayerIt)) {
				return;
			}
		}
	}

	mergeLayerInto(srcLayerIt, dstLayerIt);

	HTREEITEM itemToRemove = findTreeLayerNamed(asciiCatToDelete);
	if (itemToRemove) {
		pTree->DeleteItem(itemToRemove);
	}
}

HTREEITEM LayersList::findTreeLayerNamed(const AsciiString& nameToFind)
{
	if (!mTree) {
		return nullptr;
	}

	HTREEITEM hItem = mTree->GetRootItem();

	while (hItem) {
		if (nameToFind.compareNoCase(mTree->GetItemText(hItem)) == 0) {
			return hItem;
		}
		hItem = mTree->GetNextSiblingItem(hItem);
	}

	return nullptr;
}

HTREEITEM LayersList::findTreeObjectNamed(const AsciiString& objectToFind, HTREEITEM layerItem)
{
	if (!(layerItem && mTree)) {
		return nullptr;
	}

	HTREEITEM hItem = mTree->GetChildItem(layerItem);

	while (hItem) {
		if (objectToFind.compareNoCase(mTree->GetItemText(hItem)) == 0) {
			return hItem;
		}
		hItem = mTree->GetNextSiblingItem(hItem);
	}

	return nullptr;
}

void LayersList::OnHideShowLayer()
{
	// Hide/Show Layers List
	// Toggle the current layer
	AsciiString layerToToggle = mTree->getLastClickedLayer();
	ListLayerIt layerIt;
	if (!findLayerNamed(layerToToggle, &layerIt)) {
		return;
	}

	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (!pDoc) {
		return;
	}

	layerIt->show = !layerIt->show;
	updateObjectRenderFlags(&layerIt);
	updateTreeImages();
	//PointerTool::clearSelection();
}

void LayersList::OnOK()
{
	CTreeCtrl *pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);
	if (!pTree) {
		return;
	}

	// end editing in place with success
	TreeView_EndEditLabelNow(pTree->GetSafeHwnd(), false);
}

void LayersList::OnCancel()
{
	CTreeCtrl *pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);
	if (!pTree) {
		return;
	}

	// end editing in place with failure
	TreeView_EndEditLabelNow(pTree->GetSafeHwnd(), true);
}

void LayersList::OnMergeLayer(UINT commandID)
{
	CTreeCtrl *pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);
	if (!pTree) {
		return;
	}

	HTREEITEM item = pTree->GetSelectedItem();
	if (!item) {
		return;
	}

	int layerOffset = commandID - ID_LAYERSLIST_MERGELAYERINTO_BEGIN;
	if (layerOffset < 0) {
		return;
	}

	ListLayerIt dstlayerIt = mLayers.begin();

	while (layerOffset && dstlayerIt != mLayers.end()) {
		++dstlayerIt;
		--layerOffset;
	}

	AsciiString lastClickedLayer = mTree->getLastClickedLayer();
	ListLayerIt srclayerIt = mLayers.begin();
	while (srclayerIt != mLayers.end() && srclayerIt->layerName.compareNoCase(lastClickedLayer) != 0) {
		++srclayerIt;
	}

	if (srclayerIt == mLayers.end()) {
		return;
	}

	mergeLayerInto(srclayerIt, dstlayerIt);
}

void LayersList::OnMergeObject(UINT commandID)
{
	CTreeCtrl *pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);
	if (!pTree) {
		return;
	}

	HTREEITEM item = pTree->GetSelectedItem();
	if (!item) {
		return;
	}

	int layerOffset = commandID - ID_LAYERSLIST_MERGEOBJECTINTO_BEGIN;
	if (layerOffset < 0) {
		return;
	}

	ListLayerIt layerIt = mLayers.begin();

	while (layerOffset && layerIt != mLayers.end()) {
		++layerIt;
		--layerOffset;
	}

	AsciiString lastClickedObj = mTree->getLastClickedObject();
	MapObject *objToMove = findObjectByUID(lastClickedObj);
	if (objToMove) {
		changeMapObjectLayer(objToMove, layerIt->layerName);
	} else {
		PolygonTrigger* triggerToMove = findPolygonTriggerByUID(lastClickedObj);
		changePolygonTriggerLayer(triggerToMove, layerIt->layerName);
	}
}

void LayersList::OnSelectActiveLayer()
{
	AsciiString lastClickedLayer = mTree->getLastClickedLayer();
	ListLayerIt srclayerIt = mLayers.begin();
	while (srclayerIt != mLayers.end() && srclayerIt->layerName.compareNoCase(lastClickedLayer) != 0) {
		++srclayerIt;
	}

	if (srclayerIt == mLayers.end()) {
		return;
	}

	if (srclayerIt->layerName.compareNoCase(TheActiveLayerName.c_str()) == 0) {
		// No layer is selected as default.
		TheActiveLayerName = AsciiString::TheEmptyString.str();
	} else {
		// Select this layer to be the default as new objects and triggers are added.
		TheActiveLayerName = srclayerIt->layerName.str();
		// The active layer has to be shown.
		srclayerIt->show = true;

		updateObjectRenderFlags(&srclayerIt);
	}

	updateTreeImages();
}

void LayersList::updateObjectRenderFlags(ListLayerIt *updateLayer)
{
	ListLayerIt layerIt = *updateLayer;

	ListMapObjectPtrIt it;
	for (it = layerIt->objectsInLayer.begin(); it != layerIt->objectsInLayer.end(); ++it) {
		if (layerIt->show) {
			(*it)->clearFlag(FLAG_DONT_RENDER);
		} else {
			(*it)->setFlag(FLAG_DONT_RENDER);
		}

		CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
		pDoc->invalObject((*it));
	}

	ListPolygonTriggerPtrIt pit;
	for (pit = layerIt->polygonTriggersInLayer.begin(); pit != layerIt->polygonTriggersInLayer.end(); ++pit) {
		if (layerIt->show) {
			(*pit)->setShouldRender(true);
		} else {
			(*pit)->setShouldRender(false);
		}
		//pDoc->invalObject((*it));
	}
}

void LayersList::updateTreeImages()
{
	enum {
		SHOW_ICON,
		HIDE_ICON,
		SELECTED_ICON,
	};

	if (!mTree) {
		return;
	}


	ListLayerIt layerIt;

	for (layerIt = mLayers.begin(); layerIt != mLayers.end(); layerIt++) {

		HTREEITEM layer = findTreeLayerNamed(layerIt->layerName);
		int childIcon = SHOW_ICON;

		if (layerIt->layerName.compareNoCase(TheActiveLayerName.c_str()) == 0) {
			mTree->SetItemImage(layer, SELECTED_ICON, SELECTED_ICON);
			childIcon = SHOW_ICON;
		} else if (layerIt->show == false) {
			childIcon = HIDE_ICON;
			mTree->SetItemImage(layer, HIDE_ICON, HIDE_ICON);
		} else {
			childIcon = SHOW_ICON;
			mTree->SetItemImage(layer, SHOW_ICON, SHOW_ICON);
		}

		HTREEITEM item = mTree->GetChildItem(layer);
		while (item) {
			mTree->SetItemImage(item, childIcon, childIcon);
			item = mTree->GetNextSiblingItem(item);
		}
	}
}

void LayersList::OnMergeViewSelection(UINT commandID)
{
	int layerOffset = commandID - ID_LAYERSLIST_MERGEVIEWSELECTIONINTO_BEGIN;
	if (layerOffset < 0) {
		return;
	}

	ListLayerIt layerIt = mLayers.begin();

	while (layerOffset && layerIt != mLayers.end()) {
		++layerIt;
		--layerOffset;
	}

	MapObject *mapObject = MapObject::getFirstMapObject();

	std::stack<MapObject*> allSelectedObjects;
	while (mapObject) {
		if (mapObject->isSelected()) {
			allSelectedObjects.push(mapObject);
		}

		mapObject = mapObject->getNext();
	}

	while (!allSelectedObjects.empty()) {
		changeMapObjectLayer(allSelectedObjects.top(), layerIt->layerName);
		allSelectedObjects.pop();
	}


	PolygonTrigger *polygonTrigger = PolygonTrigger::getFirstPolygonTrigger();

	std::stack<PolygonTrigger*> allSelectedTriggers;
	while (polygonTrigger) {
		if (polygonTrigger->getSelected()) {
			allSelectedTriggers.push(polygonTrigger);
		}

		polygonTrigger = polygonTrigger->getNext();
	}

	while (!allSelectedTriggers.empty()) {
		changePolygonTriggerLayer(allSelectedTriggers.top(), layerIt->layerName);
		allSelectedTriggers.pop();
	}
}


//WST 11/23/2002
void LayersList::unselectAllMapObjects()
{
	MapObject *mapObject = MapObject::getFirstMapObject();
	while (mapObject) {
		mapObject->setSelected(false);
		mapObject = mapObject->getNext();
	}
}

void LayersList::unselectAllPolygonTriggers()
{
	PolygonTrigger *trigger = PolygonTrigger::getFirstPolygonTrigger();
	while (trigger) {
		trigger->setSelected(false);
		trigger = trigger->getNext();
	}
}


//WST 11/23/2002
Bool LayersList::findAndSelectMapObject(AsciiString selectedItemAsciiString)
{
	MapObject *mapObject = MapObject::getFirstMapObject();

	while (mapObject) {
		Bool dontcare;
		AsciiString mapObjectName = (mapObject)->getProperties()->getAsciiString(TheKey_uniqueID, &dontcare);

		if (mapObjectName.compareNoCase(selectedItemAsciiString) == 0) {
			// Found it... select this object
			mapObject->setSelected(true);
			return (true);
		}
		mapObject = mapObject->getNext();
	}

	return (false);
}


Bool LayersList::findAndSelectPolygonTrigger(AsciiString selectedItemAsciiString)
{
	PolygonTrigger *trigger = PolygonTrigger::getFirstPolygonTrigger();

	while (trigger) {
		const AsciiString& triggerName = trigger->getTriggerName();

		if (triggerName.compareNoCase(selectedItemAsciiString) == 0) {
			// Found it... select this object
			trigger->setSelected(true);
			return (true);
		}
		trigger = trigger->getNext();
	}
	return (false);
}



//WST 11/21/02 New Design team request to select objects from layer view box
void LayersList::OnSelectLayerObject()
{
	CTreeCtrl *pTree = (CTreeCtrl*) GetDlgItem(IDC_LL_TREE);
	if (!pTree) {
		return;
	}

	HTREEITEM item = pTree->GetSelectedItem();
	if (!item) {
		return;
	}


	CString selectedItemCString = pTree->GetItemText(item);
	AsciiString selectedItemAsciiString(selectedItemCString.GetBuffer(0));

	//Unselect everything on the map and get ready to find the one user wanted
	unselectAllMapObjects();
	unselectAllPolygonTriggers();

	// Lets check and see if this is a layer that user selected or individual items
	ListLayerIt srcLayerIt;
	if (findLayerNamed(selectedItemAsciiString, &srcLayerIt)) {
		// Ok, User selected the Layer, lets select all map objects in the layer
		ListMapObjectPtrIt mopIt;
		for (mopIt = srcLayerIt->objectsInLayer.begin(); mopIt != srcLayerIt->objectsInLayer.end(); ++mopIt) {
			(*mopIt)->setSelected(true);
		}

		ListPolygonTriggerPtrIt ptpIt;
		for (ptpIt = srcLayerIt->polygonTriggersInLayer.begin(); ptpIt != srcLayerIt->polygonTriggersInLayer.end(); ++ptpIt) {
			(*ptpIt)->setSelected(true);
		}
	}
	else {
		// User selected individual items
		if (!findAndSelectMapObject(selectedItemAsciiString)) {
			findAndSelectPolygonTrigger(selectedItemAsciiString);
		}
	}
}

MapObject *LayersList::findObjectByUID(AsciiString objectIDToFind)
{
	MapObject *obj = MapObject::getFirstMapObject();
	Bool exists;
	AsciiString testID;
	while (obj) {
		testID = obj->getProperties()->getAsciiString(TheKey_uniqueID, &exists);
		if (exists) {
			if (testID.compareNoCase(objectIDToFind) == 0) {
				return obj;
			}
		}
		obj = obj->getNext();
	}

	return nullptr;
}

PolygonTrigger* LayersList::findPolygonTriggerByUID(AsciiString triggerIDToFind)
{
	PolygonTrigger *trigger = PolygonTrigger::getFirstPolygonTrigger();

	while (trigger) {
		const AsciiString& triggerName = trigger->getTriggerName();

		if (triggerName.compareNoCase(triggerIDToFind) == 0) {
			return (trigger);
		}
		trigger = trigger->getNext();
	}

	return (nullptr);
}

BEGIN_MESSAGE_MAP(LayersList, CDialog)
	ON_NOTIFY(TVN_BEGINLABELEDIT, IDC_LL_TREE, OnBeginEditLabel)
	ON_NOTIFY(TVN_ENDLABELEDIT, IDC_LL_TREE, OnEndEditLabel)
	ON_COMMAND(ID_SELECTLAYEROBJECT, OnSelectLayerObject)
	ON_COMMAND(ID_INSERTNEWLAYER, OnNewLayer)
	ON_COMMAND(ID_DELETECURRENTLAYER, OnDeleteLayer)
	ON_COMMAND(ID_HIDECURRENTLAYER, OnHideShowLayer)
	ON_COMMAND(ID_SELECTACTIVELAYER, OnSelectActiveLayer)
	ON_COMMAND_RANGE(ID_LAYERSLIST_MERGELAYERINTO_BEGIN, ID_LAYERSLIST_MERGELAYERINTO_END, OnMergeLayer)
	ON_COMMAND_RANGE(ID_LAYERSLIST_MERGEOBJECTINTO_BEGIN, ID_LAYERSLIST_MERGEOBJECTINTO_END, OnMergeObject)
	ON_COMMAND_RANGE(ID_LAYERSLIST_MERGEVIEWSELECTIONINTO_BEGIN, ID_LAYERSLIST_MERGEVIEWSELECTIONINTO_END, OnMergeViewSelection)
END_MESSAGE_MAP()



// for purposes of linking.
// TheDefaultLayerName is NOT constant, because it is okay to change it.
std::string LayersList::TheDefaultLayerName = "Default Object Layer";
std::string LayersList::TheDefaultNewLayerName = "New Layer";
std::string LayersList::ThePolygonTriggerLayerName = "Default Trigger Layer";
std::string LayersList::TheActiveLayerName;
const std::string LayersList::TheUnmutableDefaultLayerName = "Default Object Layer";
LayersList *TheLayersList = nullptr;
