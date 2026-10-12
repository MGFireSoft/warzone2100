include("tests/movebench_common.js");

// A second assault, built so that the target mode matters more than in the first one.
//
// The attacker has thirty tanks and a lot of what the smart mode is about: ten anti-tank weapons, five
// bunker busters, five pieces of artillery, cannons, machine guns, lasers and a flamer. The defender has
// fourteen tanks of several kinds and propulsions, ten cyborgs of both sizes, six bunkers, five towers and
// hardpoints, three artillery pits and a short wall. The defender always chooses its targets the game's
// usual way; the target mode of the attacker is chosen outside, with the WZ_TARGETMODE environment variable,
// and applies to every unit of player 0.
//
// The attackers attack-move across the base. When they have got there and something is still standing, they
// are sent after the nearest of what is left. BATTLE lines are printed whenever the numbers change, with what
// the defender has left of each kind, so that how long each kind takes to fall can be read from the log.

const ATTACKER = 0;
const DEFENDER = 1;

const COLS = 10;
const X_LEFT = X_GAP - 5;
const Y_ATTACKER_FRONT = 6;
const Y_GOAL = 29;

const BODY = { viper: "Body1REC", cobra: "Body5REC", python: "Body9REC", cyborg: "CyborgLightBody", heavy: "CyborgHeavyBody" };
const PROP = { wheels: "wheeled01", half: "HalfTrack", tracks: "tracked01", hover: "hover01", legs: "CyborgLegs" };

// reach is the weapon's range in world units, which is what decides who stands in front
const ATTACKERS = [
	{ n: 1, body: "cobra", prop: "tracks", weap: "Flame1Mk1", reach: 384 },
	{ n: 1, body: "viper", prop: "wheels", weap: "MG1Mk1", reach: 768 },
	{ n: 1, body: "cobra", prop: "half", weap: "MG2Mk1", reach: 832 },
	{ n: 1, body: "python", prop: "tracks", weap: "MG3Mk1", reach: 960 },
	{ n: 2, body: "python", prop: "tracks", weap: "Cannon1Mk1", reach: 960 },
	{ n: 2, body: "cobra", prop: "tracks", weap: "Cannon2A-TMk1", reach: 1024 },
	{ n: 3, body: "cobra", prop: "half", weap: "Rocket-Pod", reach: 1088 },
	{ n: 3, body: "cobra", prop: "half", weap: "Rocket-LtA-T", reach: 1152 },
	{ n: 1, body: "cobra", prop: "half", weap: "Rocket-MRL", reach: 1152 },
	{ n: 5, body: "cobra", prop: "half", weap: "Rocket-BB", reach: 1280 },
	{ n: 2, body: "cobra", prop: "tracks", weap: "Laser3BEAMMk1", reach: 1664 },
	{ n: 4, body: "python", prop: "tracks", weap: "Missile-A-T", reach: 1792 },
	{ n: 2, body: "cobra", prop: "half", weap: "Mortar1Mk1", reach: 2304 },
	{ n: 1, body: "cobra", prop: "half", weap: "Mortar-Incendiary", reach: 2304 },
	{ n: 1, body: "python", prop: "tracks", weap: "Howitzer105Mk1", reach: 7040 },
];

const DEFENDERS = [
	// tanks
	{ n: 2, body: "cobra", prop: "wheels", weap: "MG2Mk1" },
	{ n: 1, body: "cobra", prop: "hover", weap: "Flame1Mk1" },
	{ n: 2, body: "cobra", prop: "half", weap: "Rocket-LtA-T" },
	{ n: 2, body: "cobra", prop: "tracks", weap: "Cannon2A-TMk1" },
	{ n: 2, body: "python", prop: "tracks", weap: "Cannon1Mk1" },
	{ n: 1, body: "python", prop: "tracks", weap: "Missile-A-T" },
	{ n: 1, body: "viper", prop: "half", weap: "Mortar1Mk1" },
	{ n: 2, body: "cobra", prop: "tracks", weap: "MG3Mk1" },
	{ n: 1, body: "python", prop: "tracks", weap: "Cannon4AUTOMk1" },
	// cyborgs
	{ n: 3, body: "cyborg", prop: "legs", weap: "CyborgChaingun" },
	{ n: 2, body: "cyborg", prop: "legs", weap: "CyborgRocket" },
	{ n: 1, body: "cyborg", prop: "legs", weap: "CyborgFlamer01" },
	{ n: 1, body: "cyborg", prop: "legs", weap: "Cyb-Wpn-Grenade" },
	// heavy cyborgs
	{ n: 1, body: "heavy", prop: "legs", weap: "Cyb-Hvywpn-Mcannon" },
	{ n: 1, body: "heavy", prop: "legs", weap: "Cyb-Hvywpn-A-T" },
	{ n: 1, body: "heavy", prop: "legs", weap: "Cyb-Hvywpn-HPV" },
];

// the base: where each structure stands, as an offset from the middle column and a row
const BASE = [
	{ id: "PillBox1", dx: -7, y: 14 },
	{ id: "PillBox4", dx: -4, y: 14 },
	{ id: "PillBox6", dx: -1, y: 14 },
	{ id: "PillBox2", dx: 2, y: 14 },
	{ id: "PillBox5", dx: 5, y: 14 },
	{ id: "PillBox3", dx: 8, y: 14 },
	{ id: "WallTower01", dx: -8, y: 16 },
	{ id: "WallTower06", dx: -3, y: 16 },
	{ id: "GuardTower1", dx: 0, y: 16 },
	{ id: "WallTower02", dx: 3, y: 16 },
	{ id: "WallTower03", dx: 7, y: 16 },
	{ id: "Emplacement-MortarPit01", dx: -5, y: 18 },
	{ id: "Emplacement-MRL-pit", dx: 0, y: 18 },
	{ id: "Emplacement-MortarPit01", dx: 5, y: 18 },
];
const WALL_Y = 12;
const WALL_PIECES = [-8, -7, -6, -3, -2, 2, 3, 6, 7, 8];

var lastReport = "";

function unitList(entries)
{
	var list = [];
	for (var g = 0; g < entries.length; g++)
	{
		for (var k = 0; k < entries[g].n; k++)
		{
			list.push(entries[g]);
		}
	}
	return list.map(function (e, i) { return { e: e, i: i }; })
	           .sort(function (a, b) { return (a.e.reach || 0) - (b.e.reach || 0) || a.i - b.i; })
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

function attackerTally()
{
	var droids = enumDroid(ATTACKER);
	var health = 0;
	for (var i = 0; i < droids.length; i++)
	{
		health += droids[i].health;
	}
	return droids.length + " health=" + Math.round(health);
}

function defenderTally()
{
	var tanks = enumDroid(DEFENDER, DROID_WEAPON).length;
	var cyborgs = enumDroid(DEFENDER, DROID_CYBORG).length;
	var bunkers = 0;
	var others = 0;
	var walls = 0;
	var structures = enumStruct(DEFENDER);
	for (var i = 0; i < structures.length; i++)
	{
		if (structures[i].stattype === WALL)
		{
			walls++;
		}
		else if (structures[i].name.indexOf("Bunker") >= 0)
		{
			bunkers++;
		}
		else
		{
			others++;
		}
	}
	return "tanks=" + tanks + " cyborgs=" + cyborgs + " bunkers=" + bunkers + " defences=" + others + " walls=" + walls;
}

function battleCheck()
{
	var line = "attacker=" + attackerTally() + " " + defenderTally();
	if (line !== lastReport)
	{
		lastReport = line;
		debug("BATTLE t=" + gameTime + " " + line);
	}
}

// Sends every idle attacker after the nearest thing the defender still has standing, walls aside.
function sweep()
{
	var attackers = enumDroid(ATTACKER);
	if (attackers.length === 0)
	{
		return;
	}
	var targets = enumDroid(DEFENDER);
	var structures = enumStruct(DEFENDER);
	for (var i = 0; i < structures.length; i++)
	{
		if (structures[i].stattype !== WALL)
		{
			targets.push(structures[i]);
		}
	}
	if (targets.length === 0)
	{
		return;
	}
	var cx = 0;
	var cy = 0;
	for (var a = 0; a < attackers.length; a++)
	{
		cx += attackers[a].x;
		cy += attackers[a].y;
	}
	cx /= attackers.length;
	cy /= attackers.length;
	var best = targets[0];
	var bestDist = 1e9;
	for (var t = 0; t < targets.length; t++)
	{
		var d = (targets[t].x - cx) * (targets[t].x - cx) + (targets[t].y - cy) * (targets[t].y - cy);
		if (d < bestDist)
		{
			bestDist = d;
			best = targets[t];
		}
	}
	for (var k = 0; k < attackers.length; k++)
	{
		if (attackers[k].order === 0)   // 0 is DORDER_NONE, which the scripts have no name for
		{
			orderDroidLoc(attackers[k], DORDER_SCOUT, best.x, best.y);
		}
	}
}

function eventStartLevel()
{
	var a = benchArrangement();
	var dxA = (a % 3) - 1;
	var dxD = (Math.floor(a / 3) % 3) - 1;

	var attackerList = unitList(ATTACKERS);
	var defenderList = unitList(DEFENDERS);

	hackNetOff();
	enableAll(ATTACKER, attackerList);
	enableAll(DEFENDER, defenderList);

	for (var w = 0; w < WALL_PIECES.length; w++)
	{
		addStructure("A0HardcreteMk1Wall", DEFENDER, (X_GAP + WALL_PIECES[w]) * 128, WALL_Y * 128);
	}
	for (var s = 0; s < BASE.length; s++)
	{
		addStructure(BASE[s].id, DEFENDER, (X_GAP + BASE[s].dx) * 128, BASE[s].y * 128);
	}

	// the attackers in ranks of ten, shortest reach in front, the howitzer at the back
	var attackers = [];
	for (var i = 0; i < attackerList.length; i++)
	{
		var u = attackerList[i];
		var d = addDroid(ATTACKER, X_LEFT + dxA + (i % COLS), Y_ATTACKER_FRONT - Math.floor(i / COLS), "Attacker " + i, BODY[u.body], PROP[u.prop], "", "", u.weap);
		if (d !== null)
		{
			attackers.push(d);
		}
	}
	// the defenders spread through the base, behind the bunkers
	var defenders = [];
	for (var j = 0; j < defenderList.length; j++)
	{
		var v = defenderList[j];
		var x = X_GAP - 9 + dxD + (j % 10) * 2;
		var y = 20 + Math.floor(j / 10) * 2;
		var e = addDroid(DEFENDER, x, y, "Defender " + j, BODY[v.body], PROP[v.prop], "", "", v.weap);
		if (e !== null)
		{
			defenders.push(e);
		}
	}
	hackNetOn();

	for (var m = 0; m < attackers.length; m++)
	{
		orderDroidLoc(attackers[m], DORDER_SCOUT, attackers[m].x, Y_GOAL);
	}
	for (var n = 0; n < defenders.length; n++)
	{
		orderDroidLoc(defenders[n], DORDER_SCOUT, defenders[n].x, defenders[n].y);
	}

	setTimer("battleCheck", 500);
	setTimer("sweep", 5000);
	debug("movebench: assault2, " + attackers.length + " attackers against " + defenders.length + " defenders and " + (BASE.length + WALL_PIECES.length) + " structures");
}
