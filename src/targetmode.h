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
 *  Target modes, for experiments.
 *
 *  A mode changes how the local player's droids pick targets (see targetweights.h). There is no way to choose one in
 *  the game: the WZ_TARGETMODE environment variable sets it for the movement benchmarks, which compare the modes, and
 *  without it every droid picks its targets the game's usual way.
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

/// For benchmarks, with WZ_TARGETLOG set: counts a shot of the local player being fired, or one that came down with
/// nothing to hit because it missed or its target was already gone.
void targetModeLogShot(unsigned player, const WEAPON_STATS *weapon, bool lost);

struct SIMPLE_OBJECT;

/// For benchmarks, with WZ_TARGETLOG set: as a shot of the local player is fired, says what the shooter is firing at and
/// what the target choice would give right now, and where each stands in the smart order of priority. When they differ,
/// something other than the target choice decided what the unit shoots at.
void targetModeLogShotCheck(const SIMPLE_OBJECT *attacker, const BASE_OBJECT *actual, int weaponSlot);

/// For benchmarks, with WZ_FIRELOG set: a line in the log for every shot fired by anyone (FIRE), every shot that came
/// down with nothing to hit (LOST), and every bit of damage done (HIT), with who, with what, at whom and how much.
/// A shot the game has already decided will miss is fired at the ground with no target, so the FIRE line also gives the
/// shooter's own target for that weapon, which is what it was aiming at.
void targetModeLogFire(const SIMPLE_OBJECT *shooter, unsigned player, const WEAPON_STATS *weapon, const BASE_OBJECT *target, int weaponSlot);
void targetModeLogLost(const SIMPLE_OBJECT *shooter, unsigned player, const WEAPON_STATS *weapon);
void targetModeLogHit(const SIMPLE_OBJECT *shooter, unsigned player, const WEAPON_STATS *weapon, const BASE_OBJECT *target,
                      unsigned before, unsigned after, bool killed);

/// The focus modes: a large bonus when the target is the one the attacker's side is concentrating on, 0 otherwise.
int targetModeFocusBonus(const DROID *attacker, const BASE_OBJECT *target);

/// The unit threat mode: what is added to the game's own weight of a target for one attacker. It is the target's danger
/// (damage per second it does to the attacker's side) times the damage per second this attacker's weapon does to it,
/// over the hitpoints it has left: how fast this unit's fire takes that danger off the field. Summed over a side, it is
/// the target's danger over the time the side takes to kill it, so each unit picks where its own weapon helps that most.
int targetModeUnitThreatBonus(const DROID *attacker, const WEAPON_STATS *weapon, const BASE_OBJECT *target);

/// Whether a droid should keep the target it has instead of looking for a better one: in the nearest mode always, in the
/// focus modes while that target is the one its side is concentrating on.
bool targetModeKeepsTarget(const DROID *attacker, const BASE_OBJECT *current);

/// Whether the smart mode refuses to shoot through an enemy wall, looking for another target instead.
bool targetModeSmartWallShots();

/// The mode that applies to a droid: the one set by WZ_TARGETMODE for the local player's droids, the game's own
/// weighting otherwise.
TargetMode targetModeOf(const DROID *psDroid);
