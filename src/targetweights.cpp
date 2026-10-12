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

/// A target closer to dying scores higher: about 1000 points at 100 hitpoints left, 330 at 500, 120 at 1500.
int weakness(int hitpoints)
{
	return 200000 / (100 + (hitpoints > 0 ? hitpoints : 0));
}

/// What the game's own formula adds up to.
int defaultWeight(const TargetTerms &t)
{
	return t.fit + t.distance + t.health + t.typeBonus;
}

/// A weapon this poor against its target is wasting its shots.
const int POOR_FIT = 50;

const int KIND_BONUS = 800;

}

int targetModeWeight(TargetMode mode, const TargetTerms &t)
{
	switch (mode)
	{
	case TargetMode::ByType:
		return 3 * t.fit + t.distance + t.health + t.typeBonus;
	case TargetMode::Nearest:
		// a constant keeps the weight positive, the game treats zero and below as no target
		return 1000 + t.distance;
	case TargetMode::Weakest:
		return t.fit / 2 + t.distance + weakness(t.hitpoints) + t.health + t.typeBonus / 2;
	case TargetMode::Structures:
		return defaultWeight(t) + (t.isStructure ? KIND_BONUS : 0);
	case TargetMode::Units:
		return defaultWeight(t) + (t.isStructure ? 0 : KIND_BONUS);
	case TargetMode::Smart:
	{
		int weight = 2 * t.fit + t.distance + t.health + t.typeBonus + weakness(t.hitpoints) / 2;
		if (t.fit < POOR_FIT)
		{
			weight /= 3;
		}
		return weight;
	}
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
	case TargetMode::Nearest: return "nearest";
	case TargetMode::Weakest: return "weakest first";
	case TargetMode::Structures: return "structures first";
	case TargetMode::Units: return "units first";
	case TargetMode::Smart: return "smart";
	case TargetMode::Count: break;
	}
	return "";
}
