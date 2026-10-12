// SPDX-License-Identifier: GPL-2.0-or-later

/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project (https://github.com/Warzone2100)

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/** \file
 *  Formations for the Ctrl+number groups.
 *
 *  A group can be given a formation: a shape, what decides who stands in front, and how tightly the
 *  units stand. When the units of such a group are sent to a place, each one is given its own
 *  destination in the formation instead of all of them going to the same spot.
 *
 *  The setting belongs to the local player's interface only. The orders it produces are ordinary
 *  per-droid move orders, which is what goes over the network, so nothing here needs to be synchronised.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

struct DROID;

/// True if the droid belongs to a group that has a formation set.
bool formationGroupActive(const DROID *psDroid);

/// Sends the droids to a place in formation. The droids are split by group, and each group is laid out
/// around (x, y) facing away from where it came from. `issue` gives one droid its own destination.
void formationIssueMoves(const std::vector<DROID *> &droids, int x, int y, const std::function<void(DROID *, int, int)> &issue);

/// Hotkey handlers. Each one changes the formation of the group the current selection belongs to,
/// and says what the formation now is.
void kf_FormationNextShape();
void kf_FormationNextSort();
void kf_FormationNextSpacing();
void kf_FormationToggleMatchSpeed();
void kf_FormationOff();
