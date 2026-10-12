include("tests/movebench_common.js");

// Two mixed forces facing each other, to compare how well each target mode fights.
//
// Both sides start from the same 24 units, with many weapons, bodies and propulsions, laid out by weapon
// range: short range in the front rank, the mortars at the back. The defender gives up two of them for two
// more towers, so it has 22 units and four towers of different kinds. The attacker adds four units, plus two
// extra tanks where the first two towers would stand, so it has 30. The defender stands its ground and always
// picks targets the game's usual way. The attacker advances with an attack-move, and its target mode is chosen
// outside, with the WZ_TARGETMODE environment variable, and applies to every unit of player 0.
//
// BATTLE lines are printed whenever the numbers change: units left on each side and the sum of their
// health, in percent of one unit each.

const ATTACKER = 0;
const DEFENDER = 1;

const COLS = 8;
const X_LEFT = X_GAP - 4;
const Y_DEFENDER_FRONT = 22;
const Y_ATTACKER_FRONT = 8;
const Y_ATTACK = 17;

const BODY = { viper: "Body1REC", cobra: "Body5REC", python: "Body9REC", cyborg: "CyborgLightBody" };
const PROP = { wheels: "wheeled01", half: "HalfTrack", tracks: "tracked01", hover: "hover01", legs: "CyborgLegs" };

// reach is the weapon's range in world units, which is what decides who stands in front
const BASE_FORCE = [
	{ n: 2, body: "cyborg", prop: "legs", weap: "CyborgFlamer01", reach: 384 },
	{ n: 1, body: "cobra", prop: "tracks", weap: "Flame1Mk1", reach: 384 },
	{ n: 2, body: "viper", prop: "wheels", weap: "MG1Mk1", reach: 768 },
	{ n: 1, body: "cobra", prop: "hover", weap: "MG1Mk1", reach: 768 },
	{ n: 2, body: "cyborg", prop: "legs", weap: "CyborgChaingun", reach: 768 },
	{ n: 2, body: "cobra", prop: "half", weap: "MG2Mk1", reach: 832 },
	{ n: 2, body: "python", prop: "tracks", weap: "MG3Mk1", reach: 960 },
	{ n: 2, body: "python", prop: "tracks", weap: "Cannon1Mk1", reach: 960 },
	{ n: 1, body: "viper", prop: "half", weap: "Cannon1Mk1", reach: 960 },
	{ n: 2, body: "cyborg", prop: "legs", weap: "CyborgRocket", reach: 960 },
	{ n: 1, body: "cobra", prop: "tracks", weap: "Cannon2A-TMk1", reach: 1024 },
	{ n: 2, body: "cobra", prop: "half", weap: "Rocket-LtA-T", reach: 1152 },
	{ n: 1, body: "cobra", prop: "half", weap: "Rocket-MRL", reach: 1152 },
	{ n: 1, body: "python", prop: "tracks", weap: "Missile-A-T", reach: 1792 },
	{ n: 2, body: "cobra", prop: "half", weap: "Mortar1Mk1", reach: 2304 },
];

// what the defender gives up for its extra towers
const DEFENDER_LOSES = [
	{ weap: "CyborgFlamer01", n: 1 },
	{ weap: "MG1Mk1", body: "viper", n: 1 },
];

// what the attacker has on top
const ATTACKER_EXTRA = [
	{ n: 1, body: "viper", prop: "wheels", weap: "MG2Mk1", reach: 832 },
	{ n: 1, body: "cobra", prop: "tracks", weap: "Cannon1Mk1", reach: 960 },
	{ n: 1, body: "cyborg", prop: "legs", weap: "CyborgChaingun", reach: 768 },
	{ n: 1, body: "cobra", prop: "half", weap: "Missile-A-T", reach: 1792 },
	// stand in for the defender's first two towers
	{ n: 1, body: "python", prop: "tracks", weap: "MG3Mk1", reach: 960 },
	{ n: 1, body: "python", prop: "tracks", weap: "Rocket-LtA-T", reach: 1152 },
];

// the defender's towers, four different kinds, and where they stand across the front
const TOWERS = [
	{ id: "WallTower01", dx: -6 },   // heavy machine gun
	{ id: "WallTower06", dx: -3 },   // lancer
	{ id: "WallTower02", dx: 2 },    // light cannon
	{ id: "GuardTower6", dx: 5 },    // mini-rockets
];

var lastReport = "";

// One entry per unit, shortest reach first. The sort is stable, so equal reaches keep the order they were listed in.
function unitList(entries, losses)
{
	var list = [];
	for (var g = 0; g < entries.length; g++)
	{
		for (var k = 0; k < entries[g].n; k++)
		{
			list.push(entries[g]);
		}
	}
	for (var l = 0; losses && l < losses.length; l++)
	{
		for (var m = 0; m < list.length; m++)
		{
			if (list[m].weap === losses[l].weap && (!losses[l].body || list[m].body === losses[l].body))
			{
				list.splice(m, 1);
				break;
			}
		}
	}
	return list.map(function (e, i) { return { e: e, i: i }; })
	           .sort(function (a, b) { return a.e.reach - b.e.reach || a.i - b.i; })
	           .map(function (x) { return x.e; });
}

function enableAll(player, list)
{
	setPower(1000000, player);
	for (var i = 0; i < list.length; i++)
	{
		makeComponentAvailable(BODY[list[i].body], player);
		makeComponentAvailable(PROP[list[i].prop], player);
		makeComponentAvailable(list[i].weap, player);
	}
}

// Lays the units out front rank first. Ranks recede in rankDir, which points away from the enemy.
function spawnForce(player, list, x0, yFront, rankDir)
{
	var made = [];
	for (var index = 0; index < list.length; index++)
	{
		var u = list[index];
		var x = x0 + (index % COLS);
		var y = yFront + Math.floor(index / COLS) * rankDir;
		var d = addDroid(player, x, y, "Duel " + index, BODY[u.body], PROP[u.prop], "", "", u.weap);
		if (d !== null)
		{
			made.push(d);
		}
	}
	return made;
}

function tally(player)
{
	var droids = enumDroid(player);
	var health = 0;
	for (var i = 0; i < droids.length; i++)
	{
		health += droids[i].health;
	}
	return droids.length + " health=" + Math.round(health);
}

function battleCheck()
{
	var line = "attacker=" + tally(ATTACKER) + " defender=" + tally(DEFENDER) + " towers=" + enumStruct(DEFENDER).length;
	if (line !== lastReport)
	{
		lastReport = line;
		debug("BATTLE t=" + gameTime + " " + line);
	}
}

function eventStartLevel()
{
	// each side's block is shifted a tile or so by the arrangement, so the same fight is run from several starts
	var a = benchArrangement();
	var dxA = (a % 3) - 1;
	var dxD = (Math.floor(a / 3) % 3) - 1;

	var attackerList = unitList(BASE_FORCE.concat(ATTACKER_EXTRA), null);
	var defenderList = unitList(BASE_FORCE, DEFENDER_LOSES);

	hackNetOff();
	enableAll(ATTACKER, attackerList);
	enableAll(DEFENDER, defenderList);

	for (var t = 0; t < TOWERS.length; t++)
	{
		addStructure(TOWERS[t].id, DEFENDER, (X_GAP + TOWERS[t].dx + dxD) * 128, (Y_DEFENDER_FRONT - 1) * 128);
	}

	var attackers = spawnForce(ATTACKER, attackerList, X_LEFT + dxA, Y_ATTACKER_FRONT, -1);
	var defenders = spawnForce(DEFENDER, defenderList, X_LEFT + dxD, Y_DEFENDER_FRONT, +1);
	hackNetOn();

	for (var i = 0; i < attackers.length; i++)
	{
		orderDroidLoc(attackers[i], DORDER_SCOUT, attackers[i].x, Y_ATTACK);
	}
	for (var j = 0; j < defenders.length; j++)
	{
		orderDroidLoc(defenders[j], DORDER_SCOUT, defenders[j].x, defenders[j].y);
	}

	setTimer("battleCheck", 500);
	debug("movebench: duel, " + attackers.length + " attackers against " + defenders.length + " defenders and " + TOWERS.length + " towers");
}
