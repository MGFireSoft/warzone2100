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

#include "lib/framework/frame.h"
#include "targetmode.h"
#include "objects.h"

#include <cstdlib>

/// For benchmarks: WZ_TARGETMODE=<number> gives every droid of the local player that mode. Without it, every droid
/// chooses its targets the game's usual way.
static int benchModeOverride()
{
	static const int mode = [] {
		const char *value = std::getenv("WZ_TARGETMODE");
		return value != nullptr ? std::atoi(value) : -1;
	}();
	return mode;
}

TargetMode targetModeOf(const DROID *psDroid)
{
	const int override = benchModeOverride();
	if (override > 0 && override < static_cast<int>(TargetMode::Count) && psDroid->player == selectedPlayer)
	{
		return static_cast<TargetMode>(override);
	}
	return TargetMode::Default;
}
