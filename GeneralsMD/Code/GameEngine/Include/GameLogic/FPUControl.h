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

// FILE: FPUControl.h /////////////////////////////////////////////////////////////////////////////
// Author: Matthew D. Campbell, June 2002
// Desc:	 Routines for controlling the FPU state
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

/**
  * setFPMode sets the FPU internal precision and rounding mode.  As DirectX is not guaranteed to
	* leave the FPU in a good state, we must call this at the start of GameLogic::update() and
	* anywhere that touches DirectX inside GameLogic loops (LoadScreen).
	*/
void setFPMode();

// GeneralsX @bugfix fbraz3 24/06/2026 Implement ScopedFPUGuard to restore FPU precision mode upon scope exit.
class ScopedFPUGuard {
public:
	ScopedFPUGuard() {}
	~ScopedFPUGuard() { setFPMode(); }
};
