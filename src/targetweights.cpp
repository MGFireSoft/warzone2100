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

#include "targetweights.h"

namespace
{

/// What the game's own formula adds up to.
int defaultWeight(const TargetTerms &t)
{
	return t.fit + t.distance + t.health + t.typeBonus;
}

bool isTank(TargetKind kind)
{
	return kind == TargetKind::HoverTank || kind == TargetKind::WheeledTank
	       || kind == TargetKind::HalftrackTank || kind == TargetKind::TrackedTank;
}

bool isStructure(TargetKind kind)
{
	return kind == TargetKind::ArtilleryStructure || kind == TargetKind::DefenseStructure || kind == TargetKind::Bunker
	       || kind == TargetKind::Wall || kind == TargetKind::OtherStructure;
}

/// How far apart the steps of the order of priority are. Wider than anything the distance and damage parts
/// can add up to, so that what a weapon is meant for always comes before what is merely closer or hurt.
const int PRIORITY_STEP = 600;
const int PRIORITY_LEVELS = 30;

/// Ignored, but not ruled out: a target with this priority is only taken when nothing else is there.
const int IGNORE = 25;

/// A weapon this poor against its target is wasting its shots.
const int POOR_FIT = 60;

}

int smartPriority(const SmartFacts &f)
{
	using K = TargetKind;
	const K k = f.kind;
	const bool tank = isTank(k);

	// Walls are not worth a shot, whatever the weapon. Only a bunker buster takes them, and only when there is
	// nothing else to break nearby.
	if (k == K::Wall && f.ruleWalls)
	{
		return (f.role == TargetRole::BunkerBuster && !f.structuresNear) ? 3 : -1;
	}

	switch (f.role)
	{
	case TargetRole::AntiTank:
		if (tank)
		{
			return 0;
		}
		// The shots are kept for tanks. While there are tanks about, and while the fighting is young enough that
		// they may still turn up, nothing else gets one, and anything the missile is poor against does not get one
		// until it is clear that no tank is coming.
		if (f.tanksKnown && f.ruleAntiTank)
		{
			return -1;
		}
		if (f.fit < POOR_FIT && f.ruleAntiTank)
		{
			return f.waitedLong ? IGNORE : -1;
		}
		switch (k)
		{
		case K::DefenseStructure: return 1;
		case K::ArtilleryStructure: return 2;
		case K::ArtilleryUnit: return 2;
		case K::SupportUnit: case K::OtherUnit: return 3;
		case K::Cyborg: return 4;
		case K::Bunker: return 5;
		case K::OtherStructure: return 6;
		default: return 9;
		}

	case TargetRole::AntiPersonnel:
		switch (k)
		{
		case K::Cyborg: case K::Bunker: return 0;
		case K::ArtilleryUnit: case K::HoverTank: case K::WheeledTank: return 2;
		case K::HalftrackTank: case K::TrackedTank: case K::SupportUnit: case K::OtherUnit: return 3;
		// heavy defences are not worth their ammunition, until nothing else is coming
		default: return (f.waitedLong || !f.ruleAntiPersonnel) ? IGNORE : -1;
		}

	case TargetRole::AllRounder:
		switch (k)
		{
		case K::HoverTank: case K::WheeledTank: case K::HalftrackTank: case K::TrackedTank:
		case K::DefenseStructure: case K::ArtilleryUnit: case K::ArtilleryStructure: return 0;
		case K::Bunker: return 1;
		case K::Cyborg: case K::SupportUnit: case K::OtherUnit: return 2;
		case K::OtherStructure: return 3;
		default: return 6;
		}

	case TargetRole::BunkerBuster:
		if (isStructure(k))
		{
			switch (k)
			{
			case K::Bunker: return 0;
			case K::DefenseStructure: case K::ArtilleryStructure: return 1;
			case K::OtherStructure: return 2;
			default: return 3;
			}
		}
		// units are not worth these shots while there is something to be broken nearby
		if (f.structuresNear && f.ruleBunkerBuster)
		{
			return -1;
		}
		if (tank || k == K::ArtilleryUnit)
		{
			return 5;
		}
		return 7;

	case TargetRole::Flamer:
		switch (k)
		{
		case K::Cyborg: return 0;
		case K::ArtilleryUnit: return 1;
		case K::Bunker: case K::HoverTank: case K::WheeledTank: case K::HalftrackTank: return 2;
		case K::TrackedTank: case K::SupportUnit: case K::OtherUnit: return 3;
		case K::ArtilleryStructure: return 4;
		case K::DefenseStructure: return 5;
		case K::OtherStructure: return 7;
		default: return 10;
		}

	case TargetRole::Artillery:
		switch (k)
		{
		case K::ArtilleryUnit: case K::ArtilleryStructure: return 0;
		case K::Cyborg: return 1;
		case K::DefenseStructure: return 2;
		case K::HoverTank: return 3;
		case K::WheeledTank: return 4;
		case K::HalftrackTank: return 5;
		case K::TrackedTank: return 6;
		case K::SupportUnit: case K::OtherUnit: case K::Bunker: return 7;
		case K::OtherStructure: return 8;
		default: return 10;
		}

	case TargetRole::IncendiaryArtillery:
	{
		const bool group = f.clusterSize >= 3;
		switch (k)
		{
		case K::Cyborg: return 0;
		case K::ArtilleryUnit: return 1;
		case K::HoverTank: case K::WheeledTank: return group ? 1 : 4;
		case K::HalftrackTank: return group ? 1 : 5;
		case K::TrackedTank: return group ? 1 : 6;
		case K::DefenseStructure: case K::ArtilleryStructure: return 2;
		case K::SupportUnit: case K::OtherUnit: case K::OtherStructure: return 5;
		case K::Bunker: return 9;
		default: return 12;
		}
	}

	case TargetRole::Other:
		break;
	}
	return 0;
}

int smartTargetWeight(const SmartFacts &facts, const TargetTerms &t)
{
	SmartFacts f = facts;
	f.fit = t.fit;
	const int priority = smartPriority(f);
	if (priority < 0)
	{
		return -1;
	}
	// inside one level of priority: what the weapon is better against, then what is closer and already hurt
	int local = t.fit / 2 + t.distance + t.health + t.typeBonus / 2;
	if (f.role == TargetRole::Other)
	{
		local = 3 * t.fit + t.distance + t.health + t.typeBonus;
	}
	const int weight = (PRIORITY_LEVELS - priority) * PRIORITY_STEP + local;
	return weight > 1 ? weight : 1;
}

int targetModeWeight(TargetMode mode, const TargetTerms &t)
{
	switch (mode)
	{
	case TargetMode::ByType:
		return 3 * t.fit + t.distance + t.health + t.typeBonus;
	case TargetMode::Smart:
		// the smart mode needs facts about the attacker and the target, see smartTargetWeight()
		return 3 * t.fit + t.distance + t.health + t.typeBonus;
	case TargetMode::Default:
	case TargetMode::Count:
		break;
	}
	return defaultWeight(t);
}

const char *targetModeName(TargetMode mode)
{
	switch (mode)
	{
	case TargetMode::Default: return "default";
	case TargetMode::ByType: return "by type (what the weapon is best against)";
	case TargetMode::Smart: return "smart (each weapon goes for what it is meant for)";
	case TargetMode::Count: break;
	}
	return "";
}
