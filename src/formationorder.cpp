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
#include "formationorder.h"
#include "game_world.h"
#include "formationlayout.h"
#include "objects.h"
#include "console.h"
#include "map.h"
#include "move.h"
#include "projectile.h"
#include "stats.h"

#include <algorithm>
#include <map>
#include <string>

namespace
{

const unsigned NUM_GROUPS = 10;

/// The shapes a group cycles through. Slot 0 is no formation at all.
struct ShapeChoice
{
	formation::Spec spec;
	const char *name;
};
const ShapeChoice shapeChoices[] =
{
	{{formation::Shape::Square, 1}, "no formation"},
	{{formation::Shape::Square, 1}, "square"},
	{{formation::Shape::Circle, 1}, "circle"},
	{{formation::Shape::Line, 2}, "line, 2 deep"},
	{{formation::Shape::Line, 3}, "line, 3 deep"},
	{{formation::Shape::Column, 1}, "column, 1 wide"},
	{{formation::Shape::Column, 2}, "column, 2 wide"},
	{{formation::Shape::Column, 3}, "column, 3 wide"},
};
const unsigned NUM_SHAPES = sizeof(shapeChoices) / sizeof(shapeChoices[0]);

struct SortChoice
{
	formation::SortBy sortBy;
	const char *name;
};
const SortChoice sortChoices[] =
{
	{formation::SortBy::Position, "keep current positions"},
	{formation::SortBy::Range, "by range, short range in front"},
	{formation::SortBy::Toughness, "by toughness, toughest in front"},
	{formation::SortBy::Speed, "by speed, fastest in front"},
};
const unsigned NUM_SORTS = sizeof(sortChoices) / sizeof(sortChoices[0]);

enum Spacing
{
	SPACING_CLOSED,   ///< almost touching
	SPACING_NORMAL,   ///< half a tile between units
	SPACING_OPEN,     ///< a whole empty tile between units
	NUM_SPACINGS,
};
const char *const spacingNames[NUM_SPACINGS] = {"closed", "normal", "open"};

/// The gap left between two units' hulls, for tanks and for the much smaller cyborgs.
const int tankGap[NUM_SPACINGS] = {10, TILE_UNITS / 2, TILE_UNITS};
const int cyborgGap[NUM_SPACINGS] = {6, TILE_UNITS / 4, TILE_UNITS / 2};

struct GroupSetting
{
	unsigned shape = 0;   ///< index into shapeChoices, 0 is off
	unsigned sort = 0;    ///< index into sortChoices
	unsigned spacing = SPACING_NORMAL;
};

GroupSetting groupSettings[NUM_GROUPS];

GroupSetting *settingFor(unsigned group)
{
	return group < NUM_GROUPS ? &groupSettings[group] : nullptr;
}

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

void describe(unsigned group)
{
	const GroupSetting &s = groupSettings[group];
	std::string text = "Group " + std::to_string(group) + ": ";
	if (s.shape == 0)
	{
		text += "no formation";
	}
	else
	{
		text += std::string("formation ") + shapeChoices[s.shape].name + ", " + sortChoices[s.sort].name + ", " + spacingNames[s.spacing] + " spacing";
	}
	addConsoleMessage(text.c_str(), DEFAULT_JUSTIFY, INFO_MESSAGE);
}

/// Changes the setting of the selected group. Says why when it cannot.
GroupSetting *selectedSetting(int &group)
{
	group = selectedGroup();
	if (group < 0)
	{
		addConsoleMessage("Formations need the selection to be one group (Ctrl+number)", DEFAULT_JUSTIFY, INFO_MESSAGE);
		return nullptr;
	}
	return settingFor(group);
}

formation::Unit unitInfo(const DROID *psDroid)
{
	formation::Unit unit;
	unit.id = psDroid->id;
	unit.pos = {psDroid->pos.x, psDroid->pos.y};
	unit.toughness = static_cast<int>(psDroid->originalBody);
	unit.speed = static_cast<int>(psDroid->baseSpeed);

	bool anyWeapon = false;
	bool allIndirect = true;
	for (int i = 0; i < psDroid->numWeaps; i++)
	{
		const WEAPON_STATS *weapon = psDroid->getWeaponStats(i);
		if (weapon == nullptr)
		{
			continue;
		}
		anyWeapon = true;
		unit.range = std::max(unit.range, static_cast<int>(weapon->upgrade[psDroid->player].maxRange));
		if (proj_Direct(weapon))
		{
			allIndirect = false;
		}
	}
	unit.artillery = anyWeapon && allIndirect;
	return unit;
}

/// One group's droids laid out around (x, y).
void layoutGroup(const std::vector<DROID *> &droids, const GroupSetting &setting, int x, int y, const std::function<void(DROID *, int, int)> &issue)
{
	std::vector<formation::Unit> units;
	units.reserve(droids.size());
	int maxRadius = 0;
	bool allCyborgs = true;
	int64_t sumX = 0;
	int64_t sumY = 0;
	for (const DROID *psDroid : droids)
	{
		units.push_back(unitInfo(psDroid));
		maxRadius = std::max(maxRadius, moveObjRadius(psDroid));
		allCyborgs = allCyborgs && psDroid->isCyborg();
		sumX += psDroid->pos.x;
		sumY += psDroid->pos.y;
	}
	const int64_t count = static_cast<int64_t>(droids.size());

	// face away from where the group is now
	const Vector2i toDest(x - static_cast<int>(sumX / count), y - static_cast<int>(sumY / count));
	const uint16_t heading = (toDest.x == 0 && toDest.y == 0) ? 0 : static_cast<uint16_t>(iAtan2(toDest));

	const int gap = allCyborgs ? cyborgGap[setting.spacing] : tankGap[setting.spacing];
	const int spacing = 2 * maxRadius + gap;

	const ShapeChoice &shape = shapeChoices[setting.shape];
	const std::vector<formation::Pt> slots = formation::makeSlots(shape.spec, droids.size(), spacing);
	const std::vector<size_t> places = formation::assign(units, slots, heading, sortChoices[setting.sort].sortBy);

	for (size_t i = 0; i < droids.size(); i++)
	{
		const formation::Pt world = formation::toWorld(slots[places[i]], {x, y}, heading);
		int wx = world.x;
		int wy = world.y;
		clip_world_offmap(gameWorld.map, &wx, &wy);
		issue(droids[i], wx, wy);
	}
}

}

bool formationGroupActive(const DROID *psDroid)
{
	const GroupSetting *setting = settingFor(psDroid->group);
	return setting != nullptr && setting->shape != 0 && !psDroid->isVtol() && !psDroid->isTransporter();
}

void formationIssueMoves(const std::vector<DROID *> &droids, int x, int y, const std::function<void(DROID *, int, int)> &issue)
{
	// std::map so that groups are handled in the same order every time
	std::map<unsigned, std::vector<DROID *>> byGroup;
	for (DROID *psDroid : droids)
	{
		byGroup[psDroid->group].push_back(psDroid);
	}
	for (const auto &entry : byGroup)
	{
		layoutGroup(entry.second, groupSettings[entry.first], x, y, issue);
	}
}

void kf_FormationNextShape()
{
	int group;
	GroupSetting *setting = selectedSetting(group);
	if (setting != nullptr)
	{
		setting->shape = (setting->shape + 1) % NUM_SHAPES;
		describe(group);
	}
}

void kf_FormationNextSort()
{
	int group;
	GroupSetting *setting = selectedSetting(group);
	if (setting != nullptr)
	{
		setting->sort = (setting->sort + 1) % NUM_SORTS;
		describe(group);
	}
}

void kf_FormationNextSpacing()
{
	int group;
	GroupSetting *setting = selectedSetting(group);
	if (setting != nullptr)
	{
		setting->spacing = (setting->spacing + 1) % NUM_SPACINGS;
		describe(group);
	}
}

void kf_FormationOff()
{
	int group;
	GroupSetting *setting = selectedSetting(group);
	if (setting != nullptr)
	{
		setting->shape = 0;
		describe(group);
	}
}
