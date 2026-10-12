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
 *  Target modes for the Ctrl+number groups.
 *
 *  A group of the local player can be given a mode that changes how its droids pick targets
 *  (see targetweights.h). For now the setting is local state, so it only affects games where
 *  every droid is simulated by this client, such as single player or against the AI. Later it is
 *  meant to become an order of each unit and factory, which is synchronised.
 */

#pragma once

#include "targetweights.h"

struct DROID;

struct BASE_OBJECT;
struct WEAPON_STATS;

/// The weight of a target for an attacker in the smart mode, or -1 to hold fire. Looks at what the attacker's weapon
/// is for, what the target is, and what the attacker knows about the enemy around it.
int targetModeSmartWeight(const DROID *attacker, const WEAPON_STATS *weapon, const BASE_OBJECT *target, const TargetTerms &terms);

/// For benchmarks: when the WZ_TARGETLOG environment variable is set, says in the log what kind of weapon chose what kind of target.
void targetModeLogChoice(const DROID *attacker, int weaponSlot, const BASE_OBJECT *target);

/// The mode that applies to a droid: its group's mode if it belongs to one of the local player's groups,
/// the game's own weighting otherwise.
TargetMode targetModeOf(const DROID *psDroid);

/// Hotkey handlers, for the group the current selection belongs to.
void kf_TargetModeNext();
void kf_TargetModeDefault();
