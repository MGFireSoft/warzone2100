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
 *  Formation layout: where each unit of a group stands around a destination.
 *
 *  Pure geometry with no dependency on droids or the map, so it can be tested on its own.
 *  Two independent questions are answered separately:
 *
 *  - makeSlots(): which places make up the formation, for a given shape and unit count.
 *  - assign(): which unit takes which place, given what the units are like (range, toughness, speed).
 *
 *  Slots are in local space, with +y towards the way the formation faces and +x to its right.
 *  They are returned front rank first, left to right within a rank, so that the unit order
 *  chosen by assign() maps straight onto them.
 *
 *  Angles follow the game's convention: the angle a points along the vector (sin a, cos a),
 *  a full turn is 65536.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace formation
{

struct Pt
{
	int32_t x = 0;
	int32_t y = 0;
};

enum class Shape
{
	Square,  ///< as close to a square as the unit count allows
	Circle,  ///< concentric rings around the centre
	Line,    ///< a wide front, `param` ranks deep
	Column,  ///< `param` units abreast, as long as it needs to be
};

struct Spec
{
	Shape shape = Shape::Square;
	int param = 1;  ///< depth for Line, width for Column, unused otherwise
};

/// What decides which units stand in front.
enum class SortBy
{
	Position,   ///< whoever is already furthest ahead stays ahead, so the units cross each other the least
	Range,      ///< short range in front, long range behind
	Toughness,  ///< the toughest in front
	Speed,      ///< the fastest in front
};

struct Unit
{
	uint32_t id = 0;
	Pt pos;             ///< current world position
	int range = 0;      ///< weapon reach, 0 for none
	int toughness = 0;  ///< hitpoints, armour or whatever should decide who takes the hits
	int speed = 0;
	bool artillery = false;  ///< indirect fire, always kept behind the rest
};

/// `count` places, `spacing` apart, centred on the origin. Front rank first, left to right within a rank.
std::vector<Pt> makeSlots(const Spec &spec, size_t count, int spacing);

/// The place each unit should take: result[i] is an index into `slots` for units[i].
/// `heading` is the way the formation will face, and decides which units are left and right of each other.
/// `slots` must come from makeSlots() with the same number of units.
std::vector<size_t> assign(const std::vector<Unit> &units, const std::vector<Pt> &slots, uint16_t heading, SortBy sortBy);

/// A local slot placed in the world: rotated to face `heading` and moved to `centre`.
Pt toWorld(Pt local, Pt centre, uint16_t heading);

}
