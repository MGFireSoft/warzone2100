include("tests/movebench_common.js");

// A small fight in which every target choice counts.
//
// The attacker has twelve Python tracked tanks: three bunker busters, three tank killers, three assault guns
// and three bombards. The defender has an assault gun hardpoint and a tank killer hardpoint on the flanks, a
// hardened sensor tower just behind the first of them, two twin assault cannon bunkers in the middle, two super
// heavy-gunner cyborgs, two super auto-cannon cyborgs and three Python tracked machine gun tanks behind the
// bunkers, and two mortar pits at the back. The defender always chooses its targets the game's usual way; the
// target mode of the attacker is chosen outside, with the WZ_TARGETMODE environment variable.
//
// The attackers attack-move across the base, and are sent after whatever is left once they have got there.
// UNIT lines list everything at the start, BATTLE lines follow the numbers on each side, and with WZ_FIRELOG
// set the game adds a FIRE, LOST or HIT line for every shot and every bit of damage.

const ATTACKER = 0;
const DEFENDER = 1;

const COLS = 6;
const X_LEFT = X_GAP - 3;
const Y_ATTACKER_FRONT = 7;
const Y_GOAL = 27;

const PYTHON = "Body9REC";
const TRACKS = "tracked01";

// reach is the weapon's range in world units, which is what decides who stands in front
const ATTACKERS = [
	{ n: 3, weap: "MG4ROTARYMk1", label: "Assault Gun", reach: 1152 },
	{ n: 3, weap: "Rocket-BB", label: "Bunker Buster", reach: 1280 },
	{ n: 3, weap: "Rocket-HvyA-T", label: "Tank Killer", reach: 1280 },
	{ n: 3, weap: "Mortar2Mk1", label: "Bombard", reach: 2304 },
];

const DEFENDER_STRUCTURES = [
	{ id: "Wall-RotMg", label: "Assault Gun Hardpoint", dx: -5, y: 16 },
	{ id: "WallTower-HvATrocket", label: "Tank Killer Hardpoint", dx: 5, y: 16 },
	{ id: "Sys-SensoTower02", label: "Hardened Sensor Tower", dx: -5, y: 17 },
	{ id: "PillBox-Cannon6", label: "Twin Assault Cannon Bunker", dx: -1, y: 16 },
	{ id: "PillBox-Cannon6", label: "Twin Assault Cannon Bunker", dx: 1, y: 16 },
	{ id: "Emplacement-MortarPit01", label: "Mortar Pit", dx: -2, y: 22 },
	{ id: "Emplacement-MortarPit01", label: "Mortar Pit", dx: 2, y: 22 },
];

const DEFENDER_UNITS = [
	{ body: "CyborgHeavyBody", prop: "CyborgLegs", weap: "Cyb-Hvywpn-Mcannon", label: "Super Heavy-Gunner Cyborg", dx: -2, y: 18 },
	{ body: "CyborgHeavyBody", prop: "CyborgLegs", weap: "Cyb-Hvywpn-Mcannon", label: "Super Heavy-Gunner Cyborg", dx: 2, y: 18 },
	{ body: "CyborgHeavyBody", prop: "CyborgLegs", weap: "Cyb-Hvywpn-Acannon", label: "Super Auto-Cannon Cyborg", dx: -1, y: 18 },
	{ body: "CyborgHeavyBody", prop: "CyborgLegs", weap: "Cyb-Hvywpn-Acannon", label: "Super Auto-Cannon Cyborg", dx: 1, y: 18 },
	{ body: PYTHON, prop: TRACKS, weap: "MG1Mk1", label: "Machinegun Python Tank", dx: -1, y: 19 },
	{ body: PYTHON, prop: TRACKS, weap: "MG1Mk1", label: "Machinegun Python Tank", dx: 0, y: 19 },
	{ body: PYTHON, prop: TRACKS, weap: "MG1Mk1", label: "Machinegun Python Tank", dx: 1, y: 19 },
];

var lastReport = "";

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

function structureTally()
{
	var structures = enumStruct(DEFENDER);
	var health = 0;
	for (var i = 0; i < structures.length; i++)
	{
		health += structures[i].health;
	}
	return structures.length + " health=" + Math.round(health);
}

function battleCheck()
{
	var line = "attacker=" + tally(ATTACKER) + " defender=" + tally(DEFENDER) + " structures=" + structureTally();
	if (line !== lastReport)
	{
		lastReport = line;
		debug("BATTLE t=" + gameTime + " " + line);
	}
}

// what is left standing on both sides, every few seconds, so the last of these shows how things ended
function snapshot()
{
	var all = enumDroid(ATTACKER).concat(enumDroid(DEFENDER), enumStruct(DEFENDER));
	var parts = [];
	for (var i = 0; i < all.length; i++)
	{
		parts.push(all[i].id + ":" + Math.round(all[i].health));
	}
	debug("STATE t=" + gameTime + " " + parts.join(" "));
}

// Sends every idle attacker after the nearest thing the defender still has standing.
function sweep()
{
	var attackers = enumDroid(ATTACKER);
	var targets = enumDroid(DEFENDER).concat(enumStruct(DEFENDER));
	if (attackers.length === 0 || targets.length === 0)
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
	var dyA = (Math.floor(a / 3) % 3) - 1;

	hackNetOff();
	setPower(1000000, ATTACKER);
	setPower(1000000, DEFENDER);
	makeComponentAvailable(PYTHON, ATTACKER);
	makeComponentAvailable(TRACKS, ATTACKER);
	for (var w = 0; w < ATTACKERS.length; w++)
	{
		makeComponentAvailable(ATTACKERS[w].weap, ATTACKER);
	}
	for (var v = 0; v < DEFENDER_UNITS.length; v++)
	{
		makeComponentAvailable(DEFENDER_UNITS[v].body, DEFENDER);
		makeComponentAvailable(DEFENDER_UNITS[v].prop, DEFENDER);
		makeComponentAvailable(DEFENDER_UNITS[v].weap, DEFENDER);
	}

	for (var s = 0; s < DEFENDER_STRUCTURES.length; s++)
	{
		var st = DEFENDER_STRUCTURES[s];
		var made = addStructure(st.id, DEFENDER, (X_GAP + st.dx) * 128, st.y * 128);
		if (made)
		{
			var placed = enumStruct(DEFENDER);
			for (var q = 0; q < placed.length; q++)
			{
				if (placed[q].x === X_GAP + st.dx && placed[q].y === st.y)
				{
					debug("UNIT id=" + placed[q].id + " side=defender kind=structure label=" + st.label);
				}
			}
		}
	}

	var defenders = [];
	for (var u = 0; u < DEFENDER_UNITS.length; u++)
	{
		var du = DEFENDER_UNITS[u];
		var dd = addDroid(DEFENDER, X_GAP + du.dx, du.y, du.label, du.body, du.prop, "", "", du.weap);
		if (dd !== null)
		{
			defenders.push(dd);
			debug("UNIT id=" + dd.id + " side=defender kind=" + (du.body === PYTHON ? "tank" : "cyborg") + " label=" + du.label);
		}
	}

	var attackers = [];
	var index = 0;
	for (var g = 0; g < ATTACKERS.length; g++)
	{
		for (var k = 0; k < ATTACKERS[g].n; k++)
		{
			var x = X_LEFT + dxA + (index % COLS);
			var y = Y_ATTACKER_FRONT + dyA - Math.floor(index / COLS);
			var ad = addDroid(ATTACKER, x, y, ATTACKERS[g].label, PYTHON, TRACKS, "", "", ATTACKERS[g].weap);
			if (ad !== null)
			{
				attackers.push(ad);
				debug("UNIT id=" + ad.id + " side=attacker kind=tank label=" + ATTACKERS[g].label);
			}
			index++;
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
	setTimer("snapshot", 5000);
	debug("movebench: precision, " + attackers.length + " attackers against " + defenders.length + " units and " + DEFENDER_STRUCTURES.length + " structures");
}
