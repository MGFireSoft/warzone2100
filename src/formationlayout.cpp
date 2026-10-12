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

#include "formationlayout.h"

#include <algorithm>
#include <cmath>

namespace formation
{

namespace
{

const double TWO_PI = 6.283185307179586;

/// Rows of `cols` places, the last row holding whatever is left over and centred. The formation is
/// centred on the origin, so the front rank sits at +y.
std::vector<Pt> gridSlots(size_t count, size_t cols, int spacing)
{
	std::vector<Pt> slots;
	if (count == 0)
	{
		return slots;
	}
	cols = std::max<size_t>(cols, 1);
	const size_t rows = (count + cols - 1) / cols;
	for (size_t i = 0; i < count; i++)
	{
		const size_t row = i / cols;
		const size_t col = i % cols;
		const size_t inRow = (row + 1 < rows) ? cols : count - cols * (rows - 1);
		// doubled so that an even rank still centres on whole numbers
		const int64_t x2 = 2 * static_cast<int64_t>(col) - static_cast<int64_t>(inRow - 1);
		const int64_t y2 = static_cast<int64_t>(rows - 1) - 2 * static_cast<int64_t>(row);
		slots.push_back({static_cast<int32_t>(x2 * spacing / 2), static_cast<int32_t>(y2 * spacing / 2)});
	}
	return slots;
}

/// A unit in the middle, then rings of 6, 12, 18... units. Each ring is one `spacing` further out,
/// which leaves its units a little over `spacing` apart.
std::vector<Pt> circleSlots(size_t count, int spacing)
{
	std::vector<Pt> slots;
	if (count == 0)
	{
		return slots;
	}
	slots.push_back({0, 0});
	for (size_t ring = 1; slots.size() < count; ring++)
	{
		const size_t inRing = 6 * ring;
		for (size_t i = 0; i < inRing && slots.size() < count; i++)
		{
			const double a = TWO_PI * static_cast<double>(i) / static_cast<double>(inRing);
			const double r = static_cast<double>(ring) * spacing;
			slots.push_back({static_cast<int32_t>(std::lround(r * std::sin(a))), static_cast<int32_t>(std::lround(r * std::cos(a)))});
		}
	}
	return slots;
}

/// The unit vector for an angle.
void headingVector(uint16_t heading, double &fx, double &fy)
{
	const double a = TWO_PI * heading / 65536.0;
	fx = std::sin(a);
	fy = std::cos(a);
}

}

std::vector<Pt> makeSlots(const Spec &spec, size_t count, int spacing)
{
	std::vector<Pt> slots;
	const int param = std::max(spec.param, 1);
	switch (spec.shape)
	{
	case Shape::Square:
		slots = gridSlots(count, static_cast<size_t>(std::ceil(std::sqrt(static_cast<double>(count)))), spacing);
		break;
	case Shape::Circle:
		slots = circleSlots(count, spacing);
		break;
	case Shape::Line:
		slots = gridSlots(count, (count + param - 1) / param, spacing);
		break;
	case Shape::Column:
		slots = gridSlots(count, static_cast<size_t>(param), spacing);
		break;
	}
	// front first, then left to right, whatever the shape
	std::stable_sort(slots.begin(), slots.end(), [](const Pt &a, const Pt &b) {
		return a.y != b.y ? a.y > b.y : a.x < b.x;
	});
	return slots;
}

std::vector<size_t> assign(const std::vector<Unit> &units, const std::vector<Pt> &slots, uint16_t heading, SortBy sortBy)
{
	const size_t n = std::min(units.size(), slots.size());
	std::vector<size_t> result(units.size(), 0);
	if (n == 0)
	{
		return result;
	}

	// where each unit is along and across the way the formation will face, relative to the group's middle
	double fx, fy;
	headingVector(heading, fx, fy);
	double cx = 0, cy = 0;
	for (size_t i = 0; i < n; i++)
	{
		cx += units[i].pos.x;
		cy += units[i].pos.y;
	}
	cx /= static_cast<double>(n);
	cy /= static_cast<double>(n);
	std::vector<double> along(n), across(n);
	for (size_t i = 0; i < n; i++)
	{
		const double dx = units[i].pos.x - cx;
		const double dy = units[i].pos.y - cy;
		along[i] = dx * fx + dy * fy;
		across[i] = dx * fy - dy * fx;  // positive is to the right of the way it faces
	}

	// who goes in front: artillery last, then the chosen criterion, ties by how far ahead the unit already is, then id
	std::vector<size_t> order(n);
	for (size_t i = 0; i < n; i++)
	{
		order[i] = i;
	}
	std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
		const Unit &ua = units[a];
		const Unit &ub = units[b];
		if (ua.artillery != ub.artillery)
		{
			return !ua.artillery;
		}
		switch (sortBy)
		{
		case SortBy::Range:
			if (ua.range != ub.range)
			{
				return ua.range < ub.range;
			}
			break;
		case SortBy::Toughness:
			if (ua.toughness != ub.toughness)
			{
				return ua.toughness > ub.toughness;
			}
			break;
		case SortBy::Position:
			break;
		}
		if (along[a] != along[b])
		{
			return along[a] > along[b];
		}
		return ua.id < ub.id;
	});

	// Units go to places front rank first. Inside a rank, the one furthest left takes the leftmost place,
	// so units do not cross over each other to reach their places.
	size_t rankStart = 0;
	while (rankStart < n)
	{
		size_t rankEnd = rankStart + 1;
		while (rankEnd < n && slots[rankEnd].y == slots[rankStart].y)
		{
			rankEnd++;
		}
		std::stable_sort(order.begin() + rankStart, order.begin() + rankEnd, [&](size_t a, size_t b) {
			return across[a] < across[b];
		});
		for (size_t k = rankStart; k < rankEnd; k++)
		{
			result[order[k]] = k;
		}
		rankStart = rankEnd;
	}
	return result;
}

Pt toWorld(Pt local, Pt centre, uint16_t heading)
{
	double fx, fy;
	headingVector(heading, fx, fy);
	// forward is (fx, fy), right is (fy, -fx)
	const double wx = local.y * fx + local.x * fy;
	const double wy = local.y * fy - local.x * fx;
	return {centre.x + static_cast<int32_t>(std::lround(wx)), centre.y + static_cast<int32_t>(std::lround(wy))};
}

}
