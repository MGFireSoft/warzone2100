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

/// A weapon this poor against its target is wasting its shots.
const int POOR_FIT = 60;

}

int targetModeWeight(TargetMode mode, const TargetTerms &t)
{
	switch (mode)
	{
	case TargetMode::ByType:
		return 3 * t.fit + t.distance + t.health + t.typeBonus;
	case TargetMode::Smart:
	{
		// The fit counts squared, so a weapon at 120% against a target is worth far more than at 60%, not twice as much.
		// A target the weapon is poor against is all but ignored, however close or hurt it is. Damage already done
		// counts double, so that what is half dead is finished before a fresh target is started on.
		int weight = t.fit * t.fit / 8 + t.distance + 2 * t.health + t.typeBonus;
		if (t.fit < POOR_FIT)
		{
			weight /= 10;
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
	case TargetMode::Smart: return "smart";
	case TargetMode::Count: break;
	}
	return "";
}
