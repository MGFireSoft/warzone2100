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
 *  How a target mode turns the parts of a target's score into one weight.
 *
 *  The game scores every possible target with the same four parts, in one currency where 13 points
 *  are one tile of distance (see ai.cpp). A mode only changes how those parts are combined, so the
 *  score of the same target can be compared across modes. Pure arithmetic with no dependency on the
 *  game, so it can be tested on its own.
 */

#pragma once

enum class TargetMode
{
	Default,     ///< the game's own weighting
	ByType,      ///< what the weapon is good against counts for much more
	Smart,       ///< what the weapon is good against decides almost everything, and a poor match is avoided
	Count,
};

/// The parts of one target's score.
struct TargetTerms
{
	int fit = 0;          ///< damage modifier of the attacker's weapon against the target, in percent
	int distance = 0;     ///< sensor range minus distance, already in points: the closer the higher
	int health = 0;       ///< points for damage the target has already taken
	int typeBonus = 0;    ///< points for what the target is (armed unit, factory, defence...)
	int hitpoints = 0;    ///< hitpoints the target has left
	bool isStructure = false;
};

/// The weight of a target under a mode. Higher is better.
int targetModeWeight(TargetMode mode, const TargetTerms &terms);

const char *targetModeName(TargetMode mode);
