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
 *  score of the same target can be compared across modes. The smart mode goes further: it knows what
 *  each kind of weapon is for and what each target is, and ranks the targets in a fixed order of
 *  priority for that weapon. Pure arithmetic with no dependency on the game, so it can be tested on
 *  its own.
 */

#pragma once

enum class TargetMode
{
	Default,     ///< the game's own weighting
	ByType,      ///< what the weapon is good against counts for much more
	Smart,       ///< each kind of weapon goes for the targets it is meant for, in a fixed order of priority
	Nearest,     ///< the first thing within reach, kept until it is destroyed
	Focus,       ///< the whole side fires at one target until it is destroyed, then at the nearest next one
	ThreatFocus, ///< the whole side fires at the most dangerous target it can reach, then the next most dangerous
	Count,
};

/// What a weapon is for.
enum class TargetRole
{
	Other,                ///< no special rules, scored like by type
	AntiTank,             ///< rockets and missiles: tanks, and the shots are kept for them
	AntiPersonnel,        ///< machine guns and anti-personnel lasers: cyborgs
	AllRounder,           ///< cannons: tanks and defences
	BunkerBuster,         ///< structures, bunkers above all
	Flamer,               ///< short range fire: cyborgs and light tanks
	Artillery,            ///< mortars, howitzers, rocket artillery: enemy artillery first
	IncendiaryArtillery,  ///< fire shells: cyborgs and tight groups of tanks
};

/// What a target is, as far as choosing who to shoot at goes.
enum class TargetKind
{
	ArtilleryUnit,       ///< a droid whose weapons are all indirect fire
	Cyborg,
	HoverTank,
	WheeledTank,
	HalftrackTank,
	TrackedTank,
	SupportUnit,         ///< sensors, repair, construction
	OtherUnit,
	ArtilleryStructure,  ///< a mortar pit or other indirect fire emplacement
	DefenseStructure,    ///< towers, hardpoints, fortresses and the like
	Bunker,
	Wall,
	OtherStructure,
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

/// What the attacker knows about its surroundings, for the smart mode.
struct SmartFacts
{
	TargetRole role = TargetRole::Other;
	TargetKind kind = TargetKind::OtherUnit;
	bool tanksKnown = false;      ///< an enemy tank is in sight, was spotted by a sensor, or was seen a little while ago
	bool structuresNear = false;  ///< an enemy structure is in sight near the attacker
	int clusterSize = 0;          ///< other enemy droids close around the target
	bool waitedLong = false;      ///< no tank has turned up for a long time since the fighting began, so waiting for one is over
	int fit = 100;                ///< damage modifier of the weapon against the target, in percent, set from the terms

	// The rules that hold fire, each of which can be switched off to see what it is worth.
	bool ruleWalls = true;        ///< no shots at walls
	bool ruleAntiTank = true;     ///< anti-tank weapons keep their shots for tanks
	bool ruleAntiPersonnel = true;  ///< anti-personnel weapons ignore heavy defences
	bool ruleBunkerBuster = true;   ///< bunker busters do not shoot units while there are structures about
};

/// The weight of a target under a mode. Higher is better. The smart mode is handled by smartTargetWeight().
int targetModeWeight(TargetMode mode, const TargetTerms &terms);

/// The weight of a target for the smart mode, or -1 when the attacker should hold its fire on it, which is
/// what rules out shooting a cyborg with an anti-tank missile while there are tanks to be had.
int smartTargetWeight(const SmartFacts &facts, const TargetTerms &terms);

/// Where a target stands in its weapon's order of priority: 0 first, up to 25 for what is only taken when nothing
/// else is there, or -1 to hold fire.
int smartPriority(const SmartFacts &facts);

const char *targetModeName(TargetMode mode);
