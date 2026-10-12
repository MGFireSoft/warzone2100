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
#include "lib/framework/object_list_iteration.h"
#include "targetmode.h"
#include "game_world.h"
#include "objects.h"
#include "console.h"

#include <string>

namespace
{

const unsigned NUM_GROUPS = 10;

TargetMode groupModes[NUM_GROUPS] = {};

/// The group the whole current selection belongs to, or -1 if it is not a single group.
int selectedGroup()
{
	int found = -1;
	for (const DROID *psDroid : gameWorld.objects.droids[selectedPlayer])
	{
		if (!psDroid->selected)
		{
			continue;
		}
		if (psDroid->group >= NUM_GROUPS)
		{
			return -1;
		}
		if (found != -1 && found != psDroid->group)
		{
			return -1;
		}
		found = psDroid->group;
	}
	return found;
}

void setGroupMode(int group, TargetMode mode)
{
	groupModes[group] = mode;
	const std::string text = "Group " + std::to_string(group) + " target mode: " + targetModeName(mode);
	addConsoleMessage(text.c_str(), DEFAULT_JUSTIFY, INFO_MESSAGE);
}

}

TargetMode targetModeOf(const DROID *psDroid)
{
	if (psDroid->player != selectedPlayer || psDroid->group >= NUM_GROUPS)
	{
		return TargetMode::Default;
	}
	return groupModes[psDroid->group];
}

void kf_TargetModeNext()
{
	const int group = selectedGroup();
	if (group < 0)
	{
		addConsoleMessage("Target modes need the selection to be one group (Ctrl+number)", DEFAULT_JUSTIFY, INFO_MESSAGE);
		return;
	}
	const unsigned next = (static_cast<unsigned>(groupModes[group]) + 1) % static_cast<unsigned>(TargetMode::Count);
	setGroupMode(group, static_cast<TargetMode>(next));
}

void kf_TargetModeDefault()
{
	const int group = selectedGroup();
	if (group < 0)
	{
		addConsoleMessage("Target modes need the selection to be one group (Ctrl+number)", DEFAULT_JUSTIFY, INFO_MESSAGE);
		return;
	}
	setGroupMode(group, TargetMode::Default);
}
