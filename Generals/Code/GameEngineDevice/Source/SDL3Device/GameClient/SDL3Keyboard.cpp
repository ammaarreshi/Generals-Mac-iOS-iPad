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
** SDL3Keyboard.cpp
**
** SDL3-based keyboard implementation for Linux builds.
**
** TheSuperHackers @feature CnC_Generals_Linux 10/02/2026 Bender
** Replaces Win32DIKeyboard with SDL3 keyboard APIs for Linux.
*/

#ifndef _WIN32

// GeneralsX @bugfix BenderAI 13/02/2026 Fix include path (fighter19 pattern)
#include "SDL3Device/GameClient/SDL3Keyboard.h"
#include <cstdio>
#include <cstring>

/**
 * Constructor
 */
SDL3Keyboard::SDL3Keyboard(void)
	: Keyboard(),
	  m_nextFreeIndex(0),
	  m_nextGetIndex(0)
{
	fprintf(stderr, "DEBUG: SDL3Keyboard::SDL3Keyboard() created\n");

	// Initialize event buffer with SDL_EVENT_FIRST sentinel
	// GeneralsX @refactor felipebraz 16/02/2026 Fighter19 pattern
	memset(m_eventBuffer, 0, sizeof(m_eventBuffer));
}

/**
 * Destructor
 */
SDL3Keyboard::~SDL3Keyboard(void)
{
	fprintf(stderr, "DEBUG: SDL3Keyboard::~SDL3Keyboard() destroyed\n");
}

/**
 * Initialize keyboard subsystem
 */
void SDL3Keyboard::init(void)
{
	fprintf(stderr, "INFO: SDL3Keyboard::init()\n");

	// Call parent init
	Keyboard::init();

	// TODO: Phase 2 - Get SDL3 keyboard state array
	// m_SDLKeyState = SDL_GetKeyboardState(&m_NumKeys);

	// Clear event buffer - Fighter19 pattern
	// GeneralsX @refactor felipebraz 16/02/2026
	memset(m_eventBuffer, 0, sizeof(m_eventBuffer));
	m_nextFreeIndex = 0;
	m_nextGetIndex = 0;

	fprintf(stderr, "INFO: SDL3Keyboard::init() complete\n");
}

/**
 * Reset keyboard to default state
 */
void SDL3Keyboard::reset(void)
{
	fprintf(stderr, "DEBUG: SDL3Keyboard::reset()\n");

	Keyboard::reset();

	// Clear event buffer - Fighter19 pattern
	// GeneralsX @refactor felipebraz 16/02/2026
	memset(m_eventBuffer, 0, sizeof(m_eventBuffer));
	m_nextFreeIndex = 0;
	m_nextGetIndex = 0;
}

/**
 * Update keyboard state (called per-frame)
 */
void SDL3Keyboard::update(void)
{
	// Call parent update (processes events from getKey())
	Keyboard::update();
}

/**
 * Get keyboard data structure
 * Called by game systems to query keyboard state
 */
KeyboardIO *SDL3Keyboard::getKeyboard(void)
{
	// For Phase 1.5, return nullptr - full implementation in Phase 2
	// TODO: Phase 2 - Return keyboard state structure
	return nullptr;
}

/**
 * Get Caps Lock state (Linux stub)
 * Returns 0 (Caps Lock not active) - Windows would query keyboard driver state
 * On Linux/SDL3, we don't track Caps Lock state yet
 * GeneralsX @build fbraz 12/02/2026 Bender
 */
Bool SDL3Keyboard::getCapsState(void)
{
	// TODO: Phase 2 - Query SDL3 keyboard modifiers for Caps Lock
	// For now, assume Caps Lock is not active
	return 0;
}

/**
 * Get next keyboard event (called by parent Keyboard::update())
 * Fighter19 pattern: check for SDL_EVENT_FIRST sentinel (means empty)
 */
void SDL3Keyboard::getKey(KeyboardIO *key)
{
	// Check if buffer is empty: if event type is SDL_EVENT_FIRST (0), slot is empty
	// GeneralsX @refactor felipebraz 16/02/2026 Fighter19 pattern: sentinel check
	if (m_eventBuffer[m_nextGetIndex].type == SDL_EVENT_FIRST) {
		key->key = KEY_NONE;
		key->status = KeyboardIO::STATUS_UNUSED;
		key->state = 0;
		key->keyDownTimeMsec = 0;
		return;
	}

	// Only handle keyboard events
	if (m_eventBuffer[m_nextGetIndex].type != SDL_EVENT_KEY_DOWN &&
	    m_eventBuffer[m_nextGetIndex].type != SDL_EVENT_KEY_UP) {
		// Mark as processed (sentinel)
		m_eventBuffer[m_nextGetIndex].type = SDL_EVENT_FIRST;
		// Advance read position
		m_nextGetIndex = (m_nextGetIndex + 1) % MAX_SDL3_KEY_EVENTS;
		// Return empty
		key->key = KEY_NONE;
		key->status = KeyboardIO::STATUS_UNUSED;
		key->state = 0;
		key->keyDownTimeMsec = 0;
		return;
	}

	// Get keyboard event from buffer
	const SDL_KeyboardEvent& keyEvent = m_eventBuffer[m_nextGetIndex].key;

	// Translate SDL3 key code to game KeyDefType
	KeyDefType keyDef = translateScanCodeToKeyVal(keyEvent.scancode);

	key->key = keyDef;
	key->status = KeyboardIO::STATUS_UNUSED;
	key->state = keyEvent.down ? KEY_STATE_DOWN : KEY_STATE_UP;
	// GeneralsX @bugfix felipebraz 01/04/2026 Use the same timer domain as Keyboard::checkKeyRepeat() to avoid immediate duplicate repeats.
	key->keyDownTimeMsec = keyEvent.down ? timeGetTime() : 0;

	// Mark this slot as empty (sentinel)
	m_eventBuffer[m_nextGetIndex].type = SDL_EVENT_FIRST;

	// Advance read position (circular buffer)
	m_nextGetIndex = (m_nextGetIndex + 1) % MAX_SDL3_KEY_EVENTS;
}

/**
 * Add raw SDL_Event to keyboard event buffer
 * Fighter19 pattern: unified method that accepts any SDL_Event
 * Called by SDL3GameEngine::serviceWindowsOS() directly from event loop
 *
 * @param event Raw SDL_Event from SDL_PollEvent()
 *
 * GeneralsX @refactor felipebraz 16/02/2026 Fighter19 event model
 */
void SDL3Keyboard::addSDLEvent(SDL_Event *event)
{
	if (!event) {
		return;
	}

	// Filter only keyboard-related events
	if (event->type != SDL_EVENT_KEY_DOWN && event->type != SDL_EVENT_KEY_UP) {
		return;  // Not a keyboard event, ignore
	}

	// GeneralsX @bugfix felipebraz 01/04/2026 SDL key-repeat is redundant with engine repeat and can duplicate edits like Backspace.
	if (event->type == SDL_EVENT_KEY_DOWN && event->key.repeat) {
		return;
	}

	// Check if buffer is full
	UnsignedInt nextFreeIndex = (m_nextFreeIndex + 1) % MAX_SDL3_KEY_EVENTS;
	if (nextFreeIndex == m_nextGetIndex) {
		fprintf(stderr, "WARNING: SDL3Keyboard::addSDLEvent() buffer full (dropped event)\n");
		return;
	}

	// Copy entire event to buffer
	m_eventBuffer[m_nextFreeIndex] = *event;

	fprintf(stderr, "DEBUG: SDL3Keyboard::addSDLEvent() type=%u index=%u\n", event->type, m_nextFreeIndex);

	// Advance write position (circular buffer)
	m_nextFreeIndex = nextFreeIndex;
}

/**
 * Add SDL3 keyboard event to buffer (LEGACY - kept for compatibility)
 * Called by SDL3GameEngine::handleKeyboardEvent()
 * TODO: Remove after transition to addSDLEvent() is complete
 */
void SDL3Keyboard::addSDL3KeyEvent(const SDL_KeyboardEvent& event)
{
	UnsignedInt nextFreeIndex = (m_nextFreeIndex + 1) % MAX_SDL3_KEY_EVENTS;

	// Check if buffer is full
	if (nextFreeIndex == m_nextGetIndex) {
		fprintf(stderr, "WARNING: SDL3Keyboard::addSDL3KeyEvent() buffer full (dropped key event)\n");
		return;
	}

	// Create SDL_Event wrapper around SDL_KeyboardEvent
	SDL_Event sdlEvent;
	memset(&sdlEvent, 0, sizeof(sdlEvent));
	sdlEvent.type = event.down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	sdlEvent.key = event;

	// Add to buffer using unified method
	addSDLEvent(&sdlEvent);
}

/**
 * Translate SDL3 scancode to game KeyDefType
 * For Phase 1.5, minimal mapping - full mapping in Phase 2
 */
KeyVal SDL3Keyboard::translateScanCodeToKeyVal(unsigned char scan)
{
	// TODO: Phase 2 - Complete SDL_Scancode → KeyDefType mapping
	// For now, return KEY_NONE (parent class will handle)

	// Quick mapping for essential keys
	// GeneralsX @bugfix felipebraz 01/04/2026 Restore editing/navigation keys required by GUI widgets.
	switch ((SDL_Scancode)scan) {
		case SDL_SCANCODE_ESCAPE: return KEY_ESC;      // GeneralsX @bugfix BenderAI 13/02/2026 Fix key constant name
		case SDL_SCANCODE_RETURN: return KEY_ENTER;    // GeneralsX @bugfix BenderAI 13/02/2026 Fix key constant name
		case SDL_SCANCODE_KP_ENTER: return KEY_KPENTER;
		case SDL_SCANCODE_SPACE: return KEY_SPACE;
		case SDL_SCANCODE_TAB: return KEY_TAB;
		case SDL_SCANCODE_BACKSPACE: return KEY_BACKSPACE;
		case SDL_SCANCODE_DELETE: return KEY_DEL;
		case SDL_SCANCODE_HOME: return KEY_HOME;
		case SDL_SCANCODE_END: return KEY_END;
		case SDL_SCANCODE_PAGEUP: return KEY_PGUP;
		case SDL_SCANCODE_PAGEDOWN: return KEY_PGDN;
		case SDL_SCANCODE_LSHIFT: return KEY_LSHIFT;
		case SDL_SCANCODE_RSHIFT: return KEY_RSHIFT;
#ifdef __APPLE__
		// GeneralsX @bugfix MrMeeseeks 16/06/2026 Map Command to Control on macOS for group binding
		case SDL_SCANCODE_LGUI: return KEY_LCTRL;
		case SDL_SCANCODE_RGUI: return KEY_RCTRL;
#endif
		case SDL_SCANCODE_LCTRL: return KEY_LCTRL;     // GeneralsX @bugfix BenderAI 13/02/2026 Fix key constant name
		case SDL_SCANCODE_RCTRL: return KEY_RCTRL;     // GeneralsX @bugfix BenderAI 13/02/2026 Fix key constant name
		case SDL_SCANCODE_LALT: return KEY_LALT;       // GeneralsX @bugfix BenderAI 13/02/2026 Fix key constant name
		case SDL_SCANCODE_RALT: return KEY_RALT;       // GeneralsX @bugfix BenderAI 13/02/2026 Fix key constant name

		// Arrow keys
		case SDL_SCANCODE_UP: return KEY_UP;
		case SDL_SCANCODE_DOWN: return KEY_DOWN;
		case SDL_SCANCODE_LEFT: return KEY_LEFT;
		case SDL_SCANCODE_RIGHT: return KEY_RIGHT;

		// Function keys
		case SDL_SCANCODE_F1: return KEY_F1;
		case SDL_SCANCODE_F2: return KEY_F2;
		case SDL_SCANCODE_F3: return KEY_F3;
		case SDL_SCANCODE_F4: return KEY_F4;
		case SDL_SCANCODE_F5: return KEY_F5;
		case SDL_SCANCODE_F6: return KEY_F6;
		case SDL_SCANCODE_F7: return KEY_F7;
		case SDL_SCANCODE_F8: return KEY_F8;
		case SDL_SCANCODE_F9: return KEY_F9;
		case SDL_SCANCODE_F10: return KEY_F10;
		case SDL_SCANCODE_F11: return KEY_F11;
		case SDL_SCANCODE_F12: return KEY_F12;

		// Numbers
		case SDL_SCANCODE_1: return KEY_1;
		case SDL_SCANCODE_2: return KEY_2;
		case SDL_SCANCODE_3: return KEY_3;
		case SDL_SCANCODE_4: return KEY_4;
		case SDL_SCANCODE_5: return KEY_5;
		case SDL_SCANCODE_6: return KEY_6;
		case SDL_SCANCODE_7: return KEY_7;
		case SDL_SCANCODE_8: return KEY_8;
		case SDL_SCANCODE_9: return KEY_9;
		case SDL_SCANCODE_0: return KEY_0;

		// Letters (A-Z)
		case SDL_SCANCODE_A: return KEY_A;
		case SDL_SCANCODE_B: return KEY_B;
		case SDL_SCANCODE_C: return KEY_C;
		case SDL_SCANCODE_D: return KEY_D;
		case SDL_SCANCODE_E: return KEY_E;
		case SDL_SCANCODE_F: return KEY_F;
		case SDL_SCANCODE_G: return KEY_G;
		case SDL_SCANCODE_H: return KEY_H;
		case SDL_SCANCODE_I: return KEY_I;
		case SDL_SCANCODE_J: return KEY_J;
		case SDL_SCANCODE_K: return KEY_K;
		case SDL_SCANCODE_L: return KEY_L;
		case SDL_SCANCODE_M: return KEY_M;
		case SDL_SCANCODE_N: return KEY_N;
		case SDL_SCANCODE_O: return KEY_O;
		case SDL_SCANCODE_P: return KEY_P;
		case SDL_SCANCODE_Q: return KEY_Q;
		case SDL_SCANCODE_R: return KEY_R;
		case SDL_SCANCODE_S: return KEY_S;
		case SDL_SCANCODE_T: return KEY_T;
		case SDL_SCANCODE_U: return KEY_U;
		case SDL_SCANCODE_V: return KEY_V;
		case SDL_SCANCODE_W: return KEY_W;
		case SDL_SCANCODE_X: return KEY_X;
		case SDL_SCANCODE_Y: return KEY_Y;
		case SDL_SCANCODE_Z: return KEY_Z;

		default:
			// TODO: Phase 2 - Complete mapping
			return KEY_NONE;
	}
}

#endif // !_WIN32
