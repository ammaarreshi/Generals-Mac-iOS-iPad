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

/*
** SDL3Mouse.h
**
** SDL3-based mouse implementation for Linux builds.
**
** TheSuperHackers @feature CnC_Generals_Linux 10/02/2026 Bender
** Replaces Win32Mouse/Win32DIMouse with SDL3 mouse APIs for Linux.
*/

#pragma once

#ifndef _WIN32

// SYSTEM INCLUDES
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <array>

// USER INCLUDES
#include "GameClient/Mouse.h"

// FORWARD REFERENCES
struct AnimatedCursor;

// SDL3Mouse ------------------------------------------------------------------
/** Mouse interface using SDL3 APIs for Linux */
//-----------------------------------------------------------------------------
class SDL3Mouse : public Mouse
{
public:
	SDL3Mouse(SDL_Window* window);
	virtual ~SDL3Mouse(void);

	// SubsystemInterface
	virtual void init(void);
	virtual void reset(void);
	virtual void update(void);
	virtual void draw(void) override;
	virtual void initCursorResources(void);

	// Mouse interface
	virtual void setCursor(MouseCursor cursor);
	virtual void setVisibility(Bool visible);
	virtual void loseFocus();
	virtual void regainFocus();

	// SDL3-specific methods
	// Fighter19 pattern: addSDLEvent() accepts raw SDL_Event directly
	// GeneralsX @refactor felipebraz 16/02/2026 Switch to fighter19 event model
	void addSDLEvent(SDL_Event *event);
	
	// Legacy methods (kept for compatibility, will be removed after validation)
	void addSDL3MouseMotionEvent(const SDL_MouseMotionEvent& event);
	void addSDL3MouseButtonEvent(const SDL_MouseButtonEvent& event);
	void addSDL3MouseWheelEvent(const SDL_MouseWheelEvent& event);

protected:
	virtual void capture(void);
	virtual void releaseCapture(void);
	virtual UnsignedByte getMouseEvent(MouseIO *result, Bool flush);

private:
	// Event translation from SDL_Event (raw format)
	// GeneralsX @refactor felipebraz 16/02/2026 Unified translation method
	void translateEvent(UnsignedInt eventIndex, MouseIO *result);

	// Scale raw SDL window coordinates to game internal resolution
	// GeneralsX @bugfix felipebraz 20/02/2026 Port fighter19 coordinate scaling fix
	static void scaleMouseCoordinates(int rawX, int rawY, Uint32 windowID, int& scaledX, int& scaledY);

	// Load cursor from ANI file (fighter19 pattern)
	// GeneralsX @bugfix BenderAI 22/02/2026 Port fighter19 cursor loading
	AnimatedCursor* loadCursorFromFile(const char* filepath);

	// Legacy translation methods (kept for compatibility)
	void translateMotionEvent(const SDL_MouseMotionEvent& event, MouseIO *result);
	void translateButtonEvent(const SDL_MouseButtonEvent& event, MouseIO *result);
	void translateWheelEvent(const SDL_MouseWheelEvent& event, MouseIO *result);

	// SDL3 event buffer - Fighter19 pattern: raw SDL_Event array with sentinels
	// GeneralsX @refactor felipebraz 16/02/2026 Use raw SDL_Event buffer like fighter19
	// GeneralsX @bugfix felipebraz 18/02/2026 Increase to 256 to match Mouse::NUM_MOUSE_EVENTS
	static const UnsignedInt MAX_SDL3_MOUSE_EVENTS = 256;
	
	SDL_Event m_eventBuffer[MAX_SDL3_MOUSE_EVENTS];
	UnsignedInt m_nextFreeIndex;  // Write position (insert new events here)
	UnsignedInt m_nextGetIndex;   // Read position (retrieve events from here)

	SDL_Window* m_Window;
	Bool m_IsCaptured;
	Bool m_IsVisible;
	Bool m_LostFocus;             // GeneralsX @bugfix felipebraz 18/02/2026 Track window focus state
	
	// Track button states for click detection
	Uint32 m_LeftButtonDownTime;
	Uint32 m_RightButtonDownTime;
	Uint32 m_MiddleButtonDownTime;
	UnsignedInt m_LastFrameNumber;       // GeneralsX @bugfix felipebraz 18/02/2026 Frame tracking for determinism
	
	ICoord2D m_LeftButtonDownPos;
	ICoord2D m_RightButtonDownPos;
	ICoord2D m_MiddleButtonDownPos;
	
	// GeneralsX @bugfix BenderAI 22/02/2026 Add cursor animation tracking
	Int m_directionFrame;         ///< current frame of directional cursor (from 0 points up)

	void setCursorDirection(MouseCursor cursor);
	MouseCursor m_lastSetCursor;
	Int m_lastSetDirectionFrame;
};

#endif // !_WIN32
