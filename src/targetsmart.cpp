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
 *  The game side of the smart target mode: what the attacker's weapon is for, what the target is, and
 *  what the attacker knows about its surroundings. The order of priority itself is in targetweights.cpp.
 */

#include "lib/framework/frame.h"
#include "lib/framework/vector.h"
#include "map.h"
#include "lib/framework/object_list_iteration.h"
#include "objects.h"
#include "targetmode.h"
#include "ai.h"
#include "game_world.h"
#include "mapgrid.h"
#include "projectile.h"
#include "stats.h"
#include "structure.h"
#include "visibility.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace
{

/// How far from the attacker an enemy tank counts as being about, and how long a sighting is remembered.
const int TANKS_ABOUT_RANGE = 20 * TILE_UNITS;
const unsigned TANK_MEMORY_TIME = 30000;
/// How long the fighting has to have gone on, and how long since a tank was last seen, before waiting for tanks is over,
/// and how long without an enemy in sight ends the fighting.
const unsigned WAIT_FOR_TANKS_TIME = 45000;
const unsigned CONTACT_LOST_TIME = 20000;
/// How far around the attacker an enemy structure counts as being near.
const int STRUCTURES_NEAR_RANGE = 15 * TILE_UNITS;
/// How close around a target other droids count towards it being a group.
const int CLUSTER_RANGE = 2 * TILE_UNITS;

/// For experiments: WZ_SMARTOFF=walls,antitank,antipersonnel,bunkerbuster,wallshots switches those rules off.
bool smartRuleOff(const char *name)
{
	static const std::string off = [] {
		const char *value = std::getenv("WZ_SMARTOFF");
		return std::string(",") + (value != nullptr ? value : "") + ",";
	}();
	return off.find(std::string(",") + name + ",") != std::string::npos;
}

TargetRole weaponRole(const WEAPON_STATS *weapon, unsigned player)
{
	switch (weapon->weaponEffect)
	{
	case WE_ANTI_PERSONNEL:
		return TargetRole::AntiPersonnel;
	case WE_ANTI_TANK:
		return TargetRole::AntiTank;
	case WE_BUNKER_BUSTER:
		return TargetRole::BunkerBuster;
	case WE_FLAMER:
		return TargetRole::Flamer;
	case WE_ANTI_AIRCRAFT:   // also what the data files call "all rounder"
		return TargetRole::AllRounder;
	case WE_ARTILLERY_ROUND:
		return (weapon->weaponClass == WC_HEAT || weapon->upgrade[player].periodicalDamage > 0)
		       ? TargetRole::IncendiaryArtillery : TargetRole::Artillery;
	default:
		return TargetRole::Other;
	}
}

bool isTankKind(TargetKind kind)
{
	return kind == TargetKind::HoverTank || kind == TargetKind::WheeledTank
	       || kind == TargetKind::HalftrackTank || kind == TargetKind::TrackedTank;
}

TargetKind droidKind(const DROID *psDroid)
{
	if (psDroid->isVtol() || psDroid->isTransporter())
	{
		return TargetKind::OtherUnit;
	}
	switch (psDroid->droidType)
	{
	case DROID_SENSOR:
	case DROID_ECM:
	case DROID_REPAIR:
	case DROID_CYBORG_REPAIR:
	case DROID_CONSTRUCT:
	case DROID_CYBORG_CONSTRUCT:
		return TargetKind::SupportUnit;
	default:
		break;
	}

	// a droid that can only fire indirectly is artillery, whatever it stands on
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
		allIndirect = allIndirect && !proj_Direct(weapon);
	}
	if (anyWeapon && allIndirect)
	{
		return TargetKind::ArtilleryUnit;
	}

	if (psDroid->isCyborg() || psDroid->droidType == DROID_PERSON)
	{
		return TargetKind::Cyborg;
	}
	switch (psDroid->getPropulsionStats()->propulsionType)
	{
	case PROPULSION_TYPE_HOVER:
		return TargetKind::HoverTank;
	case PROPULSION_TYPE_WHEELED:
		return TargetKind::WheeledTank;
	case PROPULSION_TYPE_HALF_TRACKED:
		return TargetKind::HalftrackTank;
	case PROPULSION_TYPE_TRACKED:
		return TargetKind::TrackedTank;
	case PROPULSION_TYPE_LEGGED:
		return TargetKind::Cyborg;
	default:
		return TargetKind::OtherUnit;
	}
}

TargetKind structureKind(const STRUCTURE *psStructure)
{
	switch (psStructure->pStructureType->type)
	{
	case REF_DEFENSE:
	{
		if (psStructure->pStructureType->strength == STRENGTH_BUNKER)
		{
			return TargetKind::Bunker;
		}
		bool anyWeapon = false;
		bool allIndirect = true;
		for (int i = 0; i < psStructure->numWeaps; i++)
		{
			const WEAPON_STATS *weapon = psStructure->getWeaponStats(i);
			if (weapon == nullptr)
			{
				continue;
			}
			anyWeapon = true;
			allIndirect = allIndirect && !proj_Direct(weapon);
		}
		return (anyWeapon && allIndirect) ? TargetKind::ArtilleryStructure : TargetKind::DefenseStructure;
	}
	case REF_FORTRESS:
		return TargetKind::DefenseStructure;
	case REF_WALL:
	case REF_WALLCORNER:
	case REF_GATE:
		return TargetKind::Wall;
	default:
		return TargetKind::OtherStructure;
	}
}

TargetKind kindOf(const BASE_OBJECT *psObj)
{
	if (psObj->type == OBJ_DROID)
	{
		return droidKind(static_cast<const DROID *>(psObj));
	}
	if (psObj->type == OBJ_STRUCTURE)
	{
		return structureKind(static_cast<const STRUCTURE *>(psObj));
	}
	return TargetKind::OtherStructure;
}

bool enemies(unsigned a, unsigned b)
{
	return a != b && !aiCheckAlliances(a, b);
}

/// The enemy tanks a player can see this moment, rebuilt once per game tick, and where they were when last seen.
struct TankSightings
{
	unsigned scannedAt = 0;
	bool scanned = false;
	std::vector<Vector2i> now;
	unsigned lastSeenAt = 0;
	std::vector<Vector2i> lastSeen;
	unsigned contactSince = 0;   ///< when the current fighting began, 0 when there is none
	unsigned lastEnemyAt = 0;    ///< when any enemy droid was last in sight
};

TankSightings sightings[MAX_PLAYER_SLOTS];

void refreshSightings(unsigned player)
{
	TankSightings &s = sightings[player];
	if (s.scanned && s.scannedAt == gameTime)
	{
		return;
	}
	s.scanned = true;
	s.scannedAt = gameTime;
	s.now.clear();
	bool anyEnemy = false;
	for (unsigned p = 0; p < MAX_PLAYERS; p++)
	{
		if (!enemies(p, player))
		{
			continue;
		}
		for (const DROID *psDroid : gameWorld.objects.droids[p])
		{
			if (psDroid->died || psDroid->visible[player] != UBYTE_MAX)
			{
				continue;
			}
			anyEnemy = true;
			if (isTankKind(droidKind(psDroid)))
			{
				s.now.push_back(psDroid->pos.xy());
			}
		}
	}
	if (anyEnemy)
	{
		if (s.contactSince == 0 || gameTime - s.lastEnemyAt > CONTACT_LOST_TIME)
		{
			s.contactSince = gameTime;
		}
		s.lastEnemyAt = gameTime;
	}
	if (!s.now.empty())
	{
		s.lastSeenAt = gameTime;
		s.lastSeen = s.now;
	}
}

bool anyWithin(const std::vector<Vector2i> &points, Vector2i centre, int range)
{
	for (const Vector2i &p : points)
	{
		const int64_t dx = p.x - centre.x;
		const int64_t dy = p.y - centre.y;
		if (dx * dx + dy * dy <= static_cast<int64_t>(range) * range)
		{
			return true;
		}
	}
	return false;
}

/// Whether an enemy tank is known to be about: in sight, spotted by a sensor, or seen not long ago.
bool tanksKnownNear(const DROID *attacker)
{
	refreshSightings(attacker->player);
	const TankSightings &s = sightings[attacker->player];
	const Vector2i here = attacker->pos.xy();
	if (anyWithin(s.now, here, TANKS_ABOUT_RANGE))
	{
		return true;
	}
	return gameTime - s.lastSeenAt <= TANK_MEMORY_TIME && anyWithin(s.lastSeen, here, TANKS_ABOUT_RANGE);
}

/// Whether the fighting has gone on long enough, with no tank in sight for as long, that none is coming.
bool waitedLongForTanks(const DROID *attacker)
{
	refreshSightings(attacker->player);
	const TankSightings &s = sightings[attacker->player];
	return s.contactSince != 0 && gameTime - s.contactSince > WAIT_FOR_TANKS_TIME
	       && (s.lastSeenAt == 0 || gameTime - s.lastSeenAt > WAIT_FOR_TANKS_TIME);
}

/// Whether an enemy structure other than a wall is in sight near the attacker. Asked once for each candidate target
/// of one attacker in one tick, so the last answer is kept.
bool structuresNear(const DROID *attacker)
{
	static const DROID *cachedFor = nullptr;
	static unsigned cachedAt = 0;
	static bool cachedAnswer = false;
	if (cachedFor == attacker && cachedAt == gameTime)
	{
		return cachedAnswer;
	}
	bool found = false;
	for (BASE_OBJECT *psObj : gridStartIterate(attacker->pos.x, attacker->pos.y, STRUCTURES_NEAR_RANGE))
	{
		if (psObj->type != OBJ_STRUCTURE || psObj->died || !enemies(psObj->player, attacker->player)
		    || psObj->visible[attacker->player] != UBYTE_MAX)
		{
			continue;
		}
		if (structureKind(static_cast<const STRUCTURE *>(psObj)) != TargetKind::Wall)
		{
			found = true;
			break;
		}
	}
	cachedFor = attacker;
	cachedAt = gameTime;
	cachedAnswer = found;
	return found;
}

/// How many other droids close around a target are not on its side.
int clusterAround(const BASE_OBJECT *target)
{
	int count = 0;
	for (BASE_OBJECT *psObj : gridStartIterate(target->pos.x, target->pos.y, CLUSTER_RANGE))
	{
		if (psObj != target && psObj->type == OBJ_DROID && !psObj->died && psObj->player == target->player)
		{
			count++;
		}
	}
	return count;
}

}

int targetModeSmartWeight(const DROID *attacker, const WEAPON_STATS *weapon, const BASE_OBJECT *target, const TargetTerms &terms)
{
	SmartFacts facts;
	facts.role = weaponRole(weapon, attacker->player);
	facts.kind = kindOf(target);
	facts.ruleWalls = !smartRuleOff("walls");
	facts.ruleAntiTank = !smartRuleOff("antitank");
	facts.ruleAntiPersonnel = !smartRuleOff("antipersonnel");
	facts.ruleBunkerBuster = !smartRuleOff("bunkerbuster");
	if (facts.role == TargetRole::AntiTank || facts.role == TargetRole::AntiPersonnel)
	{
		facts.waitedLong = waitedLongForTanks(attacker);
	}
	if (facts.role == TargetRole::AntiTank)
	{
		facts.tanksKnown = tanksKnownNear(attacker);
	}
	else if (facts.role == TargetRole::BunkerBuster)
	{
		facts.structuresNear = structuresNear(attacker);
	}
	else if (facts.role == TargetRole::IncendiaryArtillery)
	{
		facts.clusterSize = clusterAround(target);
	}
	return smartTargetWeight(facts, terms);
}

void targetModeLogChoice(const DROID *attacker, int weaponSlot, const BASE_OBJECT *target)
{
	static const bool enabled = std::getenv("WZ_TARGETLOG") != nullptr;
	if (!enabled || attacker->player != selectedPlayer)
	{
		return;
	}
	const WEAPON_STATS *weapon = attacker->getWeaponStats(weaponSlot);
	if (weapon == nullptr)
	{
		return;
	}
	debug(LOG_INFO, "TGT role=%d kind=%d", static_cast<int>(weaponRole(weapon, attacker->player)), static_cast<int>(kindOf(target)));
}

void targetModeLogShot(unsigned player, const WEAPON_STATS *weapon, bool lost)
{
	static const bool enabled = std::getenv("WZ_TARGETLOG") != nullptr;
	if (!enabled || player != selectedPlayer || weapon == nullptr)
	{
		return;
	}
	debug(LOG_INFO, "SHOT role=%d lost=%d", static_cast<int>(weaponRole(weapon, player)), lost ? 1 : 0);
}

bool targetModeSmartWallShots()
{
	// Off by default: measured in the assault2 scenario, refusing to shoot through enemy walls cost the attacker
	// about three tanks in thirty, because a unit with nothing else to shoot at stands silent under fire instead of
	// opening a line of fire. WZ_SMARTON=wallshots turns it on for experiments.
	static const bool on = [] {
		const char *value = std::getenv("WZ_SMARTON");
		return value != nullptr && std::string(",") + value + "," != "" && (std::string(",") + value + ",").find(",wallshots,") != std::string::npos;
	}();
	return on;
}
