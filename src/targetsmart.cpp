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
#include "objmem.h"
#include "targetmode.h"
#include "ai.h"
#include "game_world.h"
#include "mapgrid.h"
#include "projectile.h"
#include "stats.h"
#include "structure.h"
#include "visibility.h"
#include "combat.h"

#include <cstdlib>
#include <map>
#include <set>
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

namespace
{

/// Where a target stands in the smart order of priority for an attacker's weapon, -1 for hold fire.
int smartTier(const DROID *attacker, const WEAPON_STATS *weapon, const BASE_OBJECT *target)
{
	SmartFacts facts;
	facts.role = weaponRole(weapon, attacker->player);
	facts.kind = kindOf(target);
	if (target->type == OBJ_DROID)
	{
		facts.fit = asWeaponModifier[weapon->weaponEffect][static_cast<const DROID *>(target)->getPropulsionStats()->propulsionType];
	}
	else
	{
		facts.fit = 100;
	}
	facts.tanksKnown = tanksKnownNear(attacker);
	facts.structuresNear = structuresNear(attacker);
	facts.waitedLong = waitedLongForTanks(attacker);
	facts.clusterSize = clusterAround(target);
	return smartPriority(facts);
}

}

void targetModeLogShotCheck(const SIMPLE_OBJECT *attackerObj, const BASE_OBJECT *actual, int weaponSlot)
{
	static const bool enabled = std::getenv("WZ_TARGETLOG") != nullptr;
	if (!enabled || attackerObj == nullptr || actual == nullptr || attackerObj->type != OBJ_DROID || attackerObj->player != selectedPlayer)
	{
		return;
	}
	DROID *attacker = const_cast<DROID *>(static_cast<const DROID *>(attackerObj));
	const WEAPON_STATS *weapon = attacker->getWeaponStats(weaponSlot);
	if (weapon == nullptr)
	{
		return;
	}
	// asking for the best target must not disturb the unit, so what the search records about itself is put back
	const unsigned failedBefore = attacker->lastCheckNearestTargetFailed[weaponSlot];
	BASE_OBJECT *best = nullptr;
	aiBestNearestTarget(attacker, &best, weaponSlot);
	attacker->lastCheckNearestTargetFailed[weaponSlot] = failedBefore;

	debug(LOG_INFO, "SHOTCHK role=%d actual=%d best=%d tierActual=%d tierBest=%d order=%d action=%d same=%d",
	      static_cast<int>(weaponRole(weapon, attacker->player)), static_cast<int>(kindOf(actual)),
	      best != nullptr ? static_cast<int>(kindOf(best)) : -1,
	      smartTier(attacker, weapon, actual), best != nullptr ? smartTier(attacker, weapon, best) : -2,
	      static_cast<int>(attacker->order.type), static_cast<int>(attacker->action), best == actual ? 1 : 0);
}

namespace
{

bool fireLogEnabled()
{
	static const bool enabled = std::getenv("WZ_FIRELOG") != nullptr;
	return enabled;
}

unsigned idOf(const SIMPLE_OBJECT *obj)
{
	return obj != nullptr ? obj->id : 0;
}

}

void targetModeLogFire(const SIMPLE_OBJECT *shooter, unsigned player, const WEAPON_STATS *weapon, const BASE_OBJECT *target, int weaponSlot)
{
	if (!fireLogEnabled() || weapon == nullptr)
	{
		return;
	}
	const BASE_OBJECT *aim = nullptr;
	if (shooter != nullptr && weaponSlot >= 0 && weaponSlot < MAX_WEAPONS)
	{
		if (shooter->type == OBJ_DROID)
		{
			aim = static_cast<const DROID *>(shooter)->psActionTarget[weaponSlot];
		}
		else if (shooter->type == OBJ_STRUCTURE)
		{
			aim = static_cast<const STRUCTURE *>(shooter)->psTarget[weaponSlot];
		}
	}
	if (aim == nullptr)
	{
		aim = target;
	}
	debug(LOG_INFO, "FIRE t=%u p=%u src=%u w=%s tgt=%u aim=%u", gameTime, player, idOf(shooter), weapon->id.toUtf8().c_str(),
	      target != nullptr ? target->id : 0, aim != nullptr ? aim->id : 0);
}

void targetModeLogLost(const SIMPLE_OBJECT *shooter, unsigned player, const WEAPON_STATS *weapon)
{
	if (!fireLogEnabled() || weapon == nullptr)
	{
		return;
	}
	debug(LOG_INFO, "LOST t=%u p=%u src=%u w=%s", gameTime, player, idOf(shooter), weapon->id.toUtf8().c_str());
}

void targetModeLogHit(const SIMPLE_OBJECT *shooter, unsigned player, const WEAPON_STATS *weapon, const BASE_OBJECT *target,
                      unsigned before, unsigned after, bool killed)
{
	if (!fireLogEnabled() || weapon == nullptr || target == nullptr)
	{
		return;
	}
	debug(LOG_INFO, "HIT t=%u p=%u src=%u w=%s tgt=%u tp=%u dmg=%u left=%u killed=%d", gameTime, player, idOf(shooter),
	      weapon->id.toUtf8().c_str(), target->id, target->player, before > after ? before - after : 0, killed ? 0 : after, killed ? 1 : 0);
}

namespace
{

/// The target a side is concentrating on in the focus mode. Kept as an id, since the object itself may be freed once destroyed.
struct FocusState
{
	unsigned targetId = 0;
	unsigned checkedAt = 0;
	bool checked = false;
};

FocusState focusStates[MAX_PLAYER_SLOTS];

const int FOCUS_BONUS = 50000;

bool focusCandidate(const BASE_OBJECT *psObj, unsigned player)
{
	if (psObj == nullptr || psObj->died || !enemies(psObj->player, player) || psObj->visible[player] != UBYTE_MAX)
	{
		return false;
	}
	if (psObj->type == OBJ_STRUCTURE)
	{
		return structureKind(static_cast<const STRUCTURE *>(psObj)) != TargetKind::Wall;
	}
	return psObj->type == OBJ_DROID && !static_cast<const DROID *>(psObj)->isVtol();
}

/// The side's shared target: kept while it stands and can be seen, otherwise the visible enemy nearest the middle of the
/// side's droids that are in the focus mode. Worked out once per tick.
const BASE_OBJECT *focusTarget(unsigned player)
{
	FocusState &f = focusStates[player];
	if (f.checked && f.checkedAt == gameTime)
	{
		return f.targetId != 0 ? getBaseObjFromId(f.targetId) : nullptr;
	}
	f.checked = true;
	f.checkedAt = gameTime;

	if (f.targetId != 0)
	{
		const BASE_OBJECT *current = getBaseObjFromId(f.targetId);
		if (focusCandidate(current, player))
		{
			return current;
		}
		f.targetId = 0;
	}

	int64_t sumX = 0;
	int64_t sumY = 0;
	int64_t count = 0;
	for (const DROID *psDroid : gameWorld.objects.droids[player])
	{
		if (!psDroid->died && targetModeOf(psDroid) == TargetMode::Focus)
		{
			sumX += psDroid->pos.x;
			sumY += psDroid->pos.y;
			count++;
		}
	}
	if (count == 0)
	{
		return nullptr;
	}
	const Vector2i middle(static_cast<int>(sumX / count), static_cast<int>(sumY / count));

	const BASE_OBJECT *best = nullptr;
	int64_t bestDistSq = 0;
	const auto consider = [&](const BASE_OBJECT *psObj) {
		if (!focusCandidate(psObj, player))
		{
			return;
		}
		const int64_t dx = psObj->pos.x - middle.x;
		const int64_t dy = psObj->pos.y - middle.y;
		const int64_t distSq = dx * dx + dy * dy;
		if (best == nullptr || distSq < bestDistSq || (distSq == bestDistSq && psObj->id < best->id))
		{
			best = psObj;
			bestDistSq = distSq;
		}
	};
	for (unsigned p = 0; p < MAX_PLAYERS; p++)
	{
		if (!enemies(p, player))
		{
			continue;
		}
		for (const DROID *psDroid : gameWorld.objects.droids[p])
		{
			consider(psDroid);
		}
		for (const STRUCTURE *psStructure : gameWorld.objects.structures[p])
		{
			consider(psStructure);
		}
	}
	f.targetId = best != nullptr ? best->id : 0;
	return best;
}

}

namespace
{

/// How much damage an enemy can do to the side's droids in a minute: for each of its weapons that can hit the ground,
/// damage times shots per minute, times what that kind of weapon does against the side's propulsions on average.
int64_t threatOf(const BASE_OBJECT *psObj, const int modifierFor[WE_NUMEFFECTS])
{
	int numWeaps = 0;
	if (psObj->type == OBJ_DROID)
	{
		numWeaps = static_cast<const DROID *>(psObj)->numWeaps;
	}
	else if (psObj->type == OBJ_STRUCTURE)
	{
		numWeaps = static_cast<int>(static_cast<const STRUCTURE *>(psObj)->numWeaps);
	}
	int64_t threat = 0;
	for (int i = 0; i < numWeaps; i++)
	{
		const WEAPON_STATS *weapon = (psObj->type == OBJ_DROID)
		                             ? static_cast<const DROID *>(psObj)->getWeaponStats(i)
		                             : static_cast<const STRUCTURE *>(psObj)->getWeaponStats(i);
		if (weapon == nullptr || !(weapon->surfaceToAir & SHOOT_ON_GROUND) || weapon->weaponEffect >= WE_NUMEFFECTS)
		{
			continue;
		}
		threat += static_cast<int64_t>(weaponDamage(*weapon, psObj->player)) * weaponROF(*weapon, psObj->player)
		          * modifierFor[weapon->weaponEffect] / 100;
	}
	return threat;
}

/// The target a side is concentrating on in the threat focus mode, kept as an id.
FocusState threatStates[MAX_PLAYER_SLOTS];

/// The most dangerous visible enemy that at least one of the side's droids in the threat focus mode can reach. Kept
/// while it stands and someone can still reach it, worked out once per tick.
const BASE_OBJECT *threatTarget(unsigned player)
{
	FocusState &f = threatStates[player];
	if (f.checked && f.checkedAt == gameTime)
	{
		return f.targetId != 0 ? getBaseObjFromId(f.targetId) : nullptr;
	}
	f.checked = true;
	f.checkedAt = gameTime;

	// the side's droids in this mode, with how far each can shoot, and what each kind of weapon does to them on average
	struct Shooter
	{
		Vector2i pos;
		int64_t range;
	};
	std::vector<Shooter> shooters;
	int64_t modifierSum[WE_NUMEFFECTS] = {};
	for (const DROID *psDroid : gameWorld.objects.droids[player])
	{
		if (psDroid->died || targetModeOf(psDroid) != TargetMode::ThreatFocus)
		{
			continue;
		}
		const WEAPON_STATS *weapon = psDroid->getWeaponStats(0);
		if (weapon != nullptr)
		{
			shooters.push_back({psDroid->pos.xy(), static_cast<int64_t>(proj_GetLongRange(*weapon, player))});
		}
		for (int e = 0; e < WE_NUMEFFECTS; e++)
		{
			modifierSum[e] += asWeaponModifier[e][psDroid->getPropulsionStats()->propulsionType];
		}
	}
	if (shooters.empty())
	{
		f.targetId = 0;
		return nullptr;
	}
	int modifierFor[WE_NUMEFFECTS];
	for (int e = 0; e < WE_NUMEFFECTS; e++)
	{
		modifierFor[e] = static_cast<int>(modifierSum[e] / static_cast<int64_t>(shooters.size()));
	}

	const auto reachable = [&](const BASE_OBJECT *psObj) {
		for (const Shooter &sh : shooters)
		{
			const int64_t dx = psObj->pos.x - sh.pos.x;
			const int64_t dy = psObj->pos.y - sh.pos.y;
			if (dx * dx + dy * dy <= sh.range * sh.range)
			{
				return true;
			}
		}
		return false;
	};

	if (f.targetId != 0)
	{
		const BASE_OBJECT *current = getBaseObjFromId(f.targetId);
		if (focusCandidate(current, player) && reachable(current))
		{
			return current;
		}
		f.targetId = 0;
	}

	const BASE_OBJECT *best = nullptr;
	int64_t bestThreat = -1;
	const auto consider = [&](const BASE_OBJECT *psObj) {
		if (!focusCandidate(psObj, player) || !reachable(psObj))
		{
			return;
		}
		const int64_t threat = threatOf(psObj, modifierFor);
		// the most dangerous first; between equals, the one closer to dying, then the lower id so every client agrees
		if (best == nullptr || threat > bestThreat
		    || (threat == bestThreat && (psObj->body < best->body || (psObj->body == best->body && psObj->id < best->id))))
		{
			best = psObj;
			bestThreat = threat;
		}
	};
	// says in the benchmark log which target the side turned on, and how dangerous it was reckoned to be
	const auto logChoice = [&]() {
		if (best != nullptr && fireLogEnabled())
		{
			debug(LOG_INFO, "FOCUS t=%u p=%u tgt=%u threat=%lld", gameTime, player, best->id, static_cast<long long>(bestThreat));
		}
	};
	for (unsigned p = 0; p < MAX_PLAYERS; p++)
	{
		if (!enemies(p, player))
		{
			continue;
		}
		for (const DROID *psDroid : gameWorld.objects.droids[p])
		{
			consider(psDroid);
		}
		for (const STRUCTURE *psStructure : gameWorld.objects.structures[p])
		{
			consider(psStructure);
		}
	}
	f.targetId = best != nullptr ? best->id : 0;
	logChoice();
	return best;
}

/// The kill order mode. For every enemy the side can reach, two figures:
///  - its danger: the damage per second it does to the side, each shot as it would land on the side's droids
///    (after their armour and the modifiers for their propulsion and body), times its shots per second and its
///    chance to hit;
///  - the time it takes to kill: its hitpoints, less the damage already flying at it, over the damage per second
///    that the side's droids within reach of it do to it, worked out the same way.
/// The side fires at the one with the highest danger over time to kill: the damage it stops taking for each
/// second of fire spent, which is the order that keeps the total damage taken lowest (Smith's rule). The choice
/// is looked at again every second, and only changed when another target is clearly better, or the current one
/// falls or gets out of reach.
struct KillOrderState
{
	unsigned targetId = 0;
	unsigned checkedAt = 0;
	bool checked = false;
	double priority = 0;
};

KillOrderState killOrderStates[MAX_PLAYER_SLOTS];

const unsigned KILL_ORDER_RECHECK = 1000;
const double KILL_ORDER_SWITCH = 1.3;   ///< how much better another target has to be to be switched to

/// Damage per second that `weapon`, fired by `player`, does to `target`, counting the chance to hit.
double damagePerSecond(const WEAPON_STATS *weapon, unsigned player, const BASE_OBJECT *target)
{
	if (weapon == nullptr || !(weapon->surfaceToAir & SHOOT_ON_GROUND))
	{
		return 0;
	}
	const double perShot = objGuessFutureDamage(const_cast<WEAPON_STATS *>(weapon), player, const_cast<BASE_OBJECT *>(target));
	const double shotsPerSecond = weaponROF(*weapon, static_cast<int>(player)) / 60.0;
	const double hit = weaponLongHit(*weapon, static_cast<int>(player)) / 100.0;
	return perShot * shotsPerSecond * hit;
}

int weaponCount(const BASE_OBJECT *psObj)
{
	if (psObj->type == OBJ_DROID)
	{
		return static_cast<const DROID *>(psObj)->numWeaps;
	}
	if (psObj->type == OBJ_STRUCTURE)
	{
		return static_cast<int>(static_cast<const STRUCTURE *>(psObj)->numWeaps);
	}
	return 0;
}

const WEAPON_STATS *weaponOf(const BASE_OBJECT *psObj, int slot)
{
	if (psObj->type == OBJ_DROID)
	{
		return static_cast<const DROID *>(psObj)->getWeaponStats(slot);
	}
	return static_cast<const STRUCTURE *>(psObj)->getWeaponStats(slot);
}

unsigned incomingDamage(const BASE_OBJECT *psObj)
{
	if (psObj->type == OBJ_DROID)
	{
		const DROID *psDroid = static_cast<const DROID *>(psObj);
		return psDroid->expectedDamageDirect + psDroid->expectedDamageIndirect;
	}
	if (psObj->type == OBJ_STRUCTURE)
	{
		return static_cast<const STRUCTURE *>(psObj)->expectedDamage;
	}
	return 0;
}

const BASE_OBJECT *killOrderTarget(unsigned player)
{
	KillOrderState &k = killOrderStates[player];
	const BASE_OBJECT *current = k.targetId != 0 ? getBaseObjFromId(k.targetId) : nullptr;
	if (current != nullptr && !focusCandidate(current, player))
	{
		current = nullptr;
		k.targetId = 0;
	}
	if (k.checked && gameTime - k.checkedAt < KILL_ORDER_RECHECK && current != nullptr)
	{
		return current;
	}
	if (k.checked && k.checkedAt == gameTime)
	{
		return current;
	}
	k.checked = true;
	k.checkedAt = gameTime;

	std::vector<const DROID *> side;
	for (const DROID *psDroid : gameWorld.objects.droids[player])
	{
		if (!psDroid->died && targetModeOf(psDroid) == TargetMode::KillOrder)
		{
			side.push_back(psDroid);
		}
	}
	if (side.empty())
	{
		k.targetId = 0;
		return nullptr;
	}

	struct Rated
	{
		const BASE_OBJECT *obj;
		double priority;
		double danger;
		double timeToKill;
	};
	const auto rate = [&](const BASE_OBJECT *psObj, Rated &out) {
		if (!focusCandidate(psObj, player))
		{
			return false;
		}
		// what the side does to it, from the droids that can reach it
		double ourDps = 0;
		for (const DROID *psDroid : side)
		{
			for (int i = 0; i < psDroid->numWeaps; i++)
			{
				const WEAPON_STATS *weapon = psDroid->getWeaponStats(i);
				if (weapon == nullptr)
				{
					continue;
				}
				const int64_t range = proj_GetLongRange(*weapon, static_cast<int>(player));
				const int64_t dx = psObj->pos.x - psDroid->pos.x;
				const int64_t dy = psObj->pos.y - psDroid->pos.y;
				if (dx * dx + dy * dy <= range * range)
				{
					ourDps += damagePerSecond(weapon, player, psObj);
				}
			}
		}
		if (ourDps <= 0)
		{
			return false;   // nobody can reach it yet
		}
		// what it does to the side, averaged over the side's droids
		double danger = 0;
		for (int i = 0; i < weaponCount(psObj); i++)
		{
			const WEAPON_STATS *weapon = weaponOf(psObj, i);
			double sum = 0;
			for (const DROID *psDroid : side)
			{
				sum += damagePerSecond(weapon, psObj->player, psDroid);
			}
			danger += sum / static_cast<double>(side.size());
		}
		const unsigned incoming = incomingDamage(psObj);
		const double hitpoints = psObj->body > incoming ? static_cast<double>(psObj->body - incoming) : 1.0;
		out.obj = psObj;
		out.danger = danger;
		out.timeToKill = hitpoints / ourDps;
		out.priority = danger / out.timeToKill;
		return true;
	};

	Rated best{nullptr, -1, 0, 0};
	Rated currentRated{nullptr, -1, 0, 0};
	for (unsigned p = 0; p < MAX_PLAYERS; p++)
	{
		if (!enemies(p, player))
		{
			continue;
		}
		const auto consider = [&](const BASE_OBJECT *psObj) {
			Rated r{nullptr, 0, 0, 0};
			if (!rate(psObj, r))
			{
				return;
			}
			if (psObj == current)
			{
				currentRated = r;
			}
			// the highest priority; with no danger at all, the quickest to kill; then the lower id so every client agrees
			const bool better = best.obj == nullptr || r.priority > best.priority
			                    || (r.priority == best.priority && (r.timeToKill < best.timeToKill
			                        || (r.timeToKill == best.timeToKill && psObj->id < best.obj->id)));
			if (better)
			{
				best = r;
			}
		};
		for (const DROID *psDroid : gameWorld.objects.droids[p])
		{
			consider(psDroid);
		}
		for (const STRUCTURE *psStructure : gameWorld.objects.structures[p])
		{
			consider(psStructure);
		}
	}

	// keep the current target unless it is out of reach, or another one is clearly better
	if (currentRated.obj != nullptr && best.obj != currentRated.obj && best.priority <= currentRated.priority * KILL_ORDER_SWITCH)
	{
		best = currentRated;
	}
	if (best.obj != nullptr && best.obj->id != k.targetId && fireLogEnabled())
	{
		debug(LOG_INFO, "FOCUS t=%u p=%u tgt=%u threat=%lld danger=%.1f ttk=%.1f", gameTime, player, best.obj->id,
		      static_cast<long long>(best.priority * 1000), best.danger, best.timeToKill);
	}
	k.targetId = best.obj != nullptr ? best.obj->id : 0;
	k.priority = best.priority;
	return best.obj;
}

const BASE_OBJECT *sharedTarget(const DROID *attacker)
{
	switch (targetModeOf(attacker))
	{
	case TargetMode::Focus:
		return focusTarget(attacker->player);
	case TargetMode::ThreatFocus:
		return threatTarget(attacker->player);
	case TargetMode::KillOrder:
		return killOrderTarget(attacker->player);
	default:
		return nullptr;
	}
}

}

int targetModeFocusBonus(const DROID *attacker, const BASE_OBJECT *target)
{
	const BASE_OBJECT *focus = sharedTarget(attacker);
	return (focus != nullptr && focus == target) ? FOCUS_BONUS : 0;
}

bool targetModeKeepsTarget(const DROID *attacker, const BASE_OBJECT *current)
{
	if (current == nullptr || current->died)
	{
		return false;
	}
	switch (targetModeOf(attacker))
	{
	case TargetMode::Nearest:
		return true;
	case TargetMode::Focus:
	case TargetMode::ThreatFocus:
	case TargetMode::KillOrder:
		return sharedTarget(attacker) == current;
	default:
		return false;
	}
}

namespace
{

/// How much the unit threat figure weighs against the game's own weight, which is a few hundred points.
const double UNIT_THREAT_SCALE = 5000.0;

/// A target's danger to a side, worked out once per tick per target and side.
struct DangerMemo
{
	unsigned time = 0;
	double danger = 0;
};

std::map<std::pair<unsigned, unsigned>, DangerMemo> dangerMemos;
std::set<unsigned> dangerLogged;

/// Damage per second `target` does to `player`'s droids, averaged over them.
double dangerTo(unsigned player, const BASE_OBJECT *target)
{
	DangerMemo &memo = dangerMemos[{player, target->id}];
	if (memo.time == gameTime && gameTime != 0)
	{
		return memo.danger;
	}
	double danger = 0;
	unsigned count = 0;
	for (const DROID *psDroid : gameWorld.objects.droids[player])
	{
		if (psDroid->died)
		{
			continue;
		}
		count++;
		for (int i = 0; i < weaponCount(target); i++)
		{
			danger += damagePerSecond(weaponOf(target, i), target->player, psDroid);
		}
	}
	memo.time = gameTime;
	memo.danger = count != 0 ? danger / count : 0;
	if (fireLogEnabled() && dangerLogged.insert(target->id).second)
	{
		debug(LOG_INFO, "DANGER t=%u p=%u tgt=%u danger=%.2f hp=%u", gameTime, player, target->id, memo.danger, target->body);
	}
	return memo.danger;
}

}

int targetModeUnitThreatBonus(const DROID *attacker, const WEAPON_STATS *weapon, const BASE_OBJECT *target)
{
	if (attacker == nullptr || weapon == nullptr || target == nullptr || target->died)
	{
		return 0;
	}
	const double danger = dangerTo(attacker->player, target);
	if (danger <= 0)
	{
		return 0;
	}
	const double myDps = damagePerSecond(weapon, attacker->player, target);
	const unsigned incoming = incomingDamage(target);
	const double hitpoints = target->body > incoming ? static_cast<double>(target->body - incoming) : 1.0;
	const double bonus = danger * myDps / hitpoints * UNIT_THREAT_SCALE;
	return bonus < 1e9 ? static_cast<int>(bonus) : 1000000000;
}
