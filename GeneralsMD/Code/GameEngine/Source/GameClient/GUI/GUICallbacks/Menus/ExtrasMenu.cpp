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

// FILE: ExtrasMenu.cpp ////////////////////////////////////////////////////////////////////////////
// Desc:   Extras/QoL settings menu for SagePatch (camera, scroll, terrain, graphics)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "GameClient/GameClient.h"
#include "Common/GlobalData.h"
#include "Common/OptionPreferences.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/Shell.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetSlider.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/Display.h"

// Widget IDs and pointers
static NameKeyType sliderMaxCameraHeightID = NAMEKEY_INVALID;
static NameKeyType sliderMinCameraHeightID = NAMEKEY_INVALID;
static NameKeyType sliderCameraPitchID = NAMEKEY_INVALID;
static NameKeyType sliderScrollSpeedID = NAMEKEY_INVALID;
static NameKeyType sliderDrawDistanceID = NAMEKEY_INVALID;

static GameWindow *sliderMaxCameraHeight = nullptr;
static GameWindow *sliderMinCameraHeight = nullptr;
static GameWindow *sliderCameraPitch = nullptr;
static GameWindow *sliderScrollSpeed = nullptr;
static GameWindow *sliderDrawDistance = nullptr;

static OptionPreferences *pref = nullptr;

// Slider ranges (matching .wnd definitions)
static const Int SLIDER_MAX_CAMERA_HEIGHT_MIN = 100;
static const Int SLIDER_MAX_CAMERA_HEIGHT_MAX = 1000;
static const Int SLIDER_MIN_CAMERA_HEIGHT_MIN = 50;
static const Int SLIDER_MIN_CAMERA_HEIGHT_MAX = 300;
static const Int SLIDER_CAMERA_PITCH_MIN = 20;
static const Int SLIDER_CAMERA_PITCH_MAX = 60;
static const Int SLIDER_SCROLL_SPEED_MIN = 1;
static const Int SLIDER_SCROLL_SPEED_MAX = 200;
static const Int SLIDER_DRAW_DISTANCE_MIN = 100;
static const Int SLIDER_DRAW_DISTANCE_MAX = 200;

//-------------------------------------------------------------------------------------------------
void ExtrasMenuInit(WindowLayout *layout, void *userData)
{
	sliderMaxCameraHeightID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:SliderMaxCameraHeight");
	sliderMinCameraHeightID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:SliderMinCameraHeight");
	sliderCameraPitchID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:SliderCameraPitch");
	sliderScrollSpeedID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:SliderScrollSpeed");
	sliderDrawDistanceID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:SliderDrawDistance");

	sliderMaxCameraHeight = TheWindowManager->winGetWindowFromId(nullptr, sliderMaxCameraHeightID);
	sliderMinCameraHeight = TheWindowManager->winGetWindowFromId(nullptr, sliderMinCameraHeightID);
	sliderCameraPitch = TheWindowManager->winGetWindowFromId(nullptr, sliderCameraPitchID);
	sliderScrollSpeed = TheWindowManager->winGetWindowFromId(nullptr, sliderScrollSpeedID);
	sliderDrawDistance = TheWindowManager->winGetWindowFromId(nullptr, sliderDrawDistanceID);

	pref = NEW OptionPreferences;

	// Populate sliders from current settings
	if (sliderMaxCameraHeight) {
		Int val = (Int)pref->getMaxCameraHeight();
		GadgetSliderSetPosition(sliderMaxCameraHeight, val);
	}
	if (sliderMinCameraHeight) {
		Int val = (Int)pref->getMinCameraHeight();
		GadgetSliderSetPosition(sliderMinCameraHeight, val);
	}
	if (sliderCameraPitch) {
		Int val = (Int)pref->getCameraPitch();
		GadgetSliderSetPosition(sliderCameraPitch, val);
	}
	if (sliderScrollSpeed) {
		Int val = (Int)(pref->getScrollFactor() * 100.0f);
		GadgetSliderSetPosition(sliderScrollSpeed, val);
	}
	if (sliderDrawDistance) {
		Int val = (Int)(pref->getTerrainDrawDistanceScale() * 100.0f);
		GadgetSliderSetPosition(sliderDrawDistance, val);
	}
}

//-------------------------------------------------------------------------------------------------
void ExtrasMenuUpdate(WindowLayout *layout, void *userData)
{
}

//-------------------------------------------------------------------------------------------------
void ExtrasMenuShutdown(WindowLayout *layout, void *userData)
{
	sliderMaxCameraHeight = nullptr;
	sliderMinCameraHeight = nullptr;
	sliderCameraPitch = nullptr;
	sliderScrollSpeed = nullptr;
	sliderDrawDistance = nullptr;

	if (pref) {
		delete pref;
		pref = nullptr;
	}

	layout->hide(TRUE);
	TheShell->shutdownComplete(layout);
}

//-------------------------------------------------------------------------------------------------
static void saveExtras()
{
	if (!pref)
		return;

	Int val;

	val = GadgetSliderGetPosition(sliderMaxCameraHeight);
	if (val > 0) {
		TheWritableGlobalData->m_maxCameraHeight = (Real)val;
		AsciiString prefString;
		prefString.format("%d", val);
		(*pref)["MaxCameraHeight"] = prefString;
	}

	val = GadgetSliderGetPosition(sliderMinCameraHeight);
	if (val > 0) {
		TheWritableGlobalData->m_minCameraHeight = (Real)val;
		AsciiString prefString;
		prefString.format("%d", val);
		(*pref)["MinCameraHeight"] = prefString;
	}

	val = GadgetSliderGetPosition(sliderCameraPitch);
	if (val > 0) {
		TheWritableGlobalData->m_cameraPitch = (Real)val;
		AsciiString prefString;
		prefString.format("%d", val);
		(*pref)["CameraPitch"] = prefString;
	}

	val = GadgetSliderGetPosition(sliderScrollSpeed);
	if (val > 0) {
		TheWritableGlobalData->m_keyboardScrollFactor = val / 100.0f;
		AsciiString prefString;
		prefString.format("%d", val);
		(*pref)["ScrollFactor"] = prefString;
	}

	val = GadgetSliderGetPosition(sliderDrawDistance);
	if (val > 0) {
		TheWritableGlobalData->m_terrainDrawDistanceScale = val / 100.0f;
		AsciiString prefString;
		prefString.format("%d", val);
		(*pref)["TerrainDrawDistanceScale"] = prefString;
	}
}

//-------------------------------------------------------------------------------------------------
static void setDefaults()
{
	if (sliderMaxCameraHeight)
		GadgetSliderSetPosition(sliderMaxCameraHeight, 500);
	if (sliderMinCameraHeight)
		GadgetSliderSetPosition(sliderMinCameraHeight, 80);
	if (sliderCameraPitch)
		GadgetSliderSetPosition(sliderCameraPitch, 37);
	if (sliderScrollSpeed)
		GadgetSliderSetPosition(sliderScrollSpeed, 100);
	if (sliderDrawDistance)
		GadgetSliderSetPosition(sliderDrawDistance, 105);
}

//-------------------------------------------------------------------------------------------------
WindowMsgHandledType ExtrasMenuSystem(GameWindow *window, UnsignedInt msg,
																				WindowMsgData mData1, WindowMsgData mData2)
{
	static NameKeyType buttonBack = NAMEKEY_INVALID;
	static NameKeyType buttonDefaults = NAMEKEY_INVALID;
	static NameKeyType buttonAccept = NAMEKEY_INVALID;

	switch (msg) {

		case GWM_CREATE:
		{
			buttonBack = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:ButtonBack");
			buttonDefaults = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:ButtonDefaults");
			buttonAccept = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:ButtonAccept");
			break;
		}

		case GWM_DESTROY:
			break;

		case GWM_INPUT_FOCUS:
		{
			if (mData1 == TRUE)
				*(Bool *)mData2 = TRUE;
			return MSG_HANDLED;
		}

		case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();

			if (controlID == buttonBack) {
				TheShell->pop();
			}
			else if (controlID == buttonAccept) {
				saveExtras();
				if (pref) {
					pref->write();
				}
				TheShell->pop();
			}
			else if (controlID == buttonDefaults) {
				setDefaults();
			}
			break;
		}

		default:
			break;
	}

	return MSG_IGNORED;
}

//-------------------------------------------------------------------------------------------------
WindowMsgHandledType ExtrasMenuInput(GameWindow *window, UnsignedInt msg,
																				WindowMsgData mData1, WindowMsgData mData2)
{
	switch (msg) {

		case GWM_CHAR:
		{
			UnsignedByte key = mData1;
			UnsignedByte state = mData2;

			switch (key) {

				case KEY_ESC:
				{
					if (BitIsSet(state, KEY_STATE_UP)) {
						TheShell->pop();
					}
					return MSG_HANDLED;
				}

			}
		}

	}

	return MSG_IGNORED;
}
