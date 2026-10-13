include("tests/movebench_common.js");

// The precision fight with artillery on both sides and the sensors that guide it.
//
// The attacker has the twelve Python tracked tanks of the precision scenario (three bunker busters, three tank killers,
// three assault guns, three bombards), a Python tracked tank with a wide spectrum sensor behind the assault tanks, and
// two Python tracked Hellstorm howitzers at the back, which stay where they are. The defender has the base of the
// precision scenario, a sensor tower in front of everything, its hardened sensor tower behind the assault gun hardpoint
// now walled in with concrete, and four Hellstorm emplacements far behind the cliff, out of reach of anything but
// the attacker's Hellstorms. Without a sensor that sees the attackers, those emplacements cannot fire at them.
//
// The defender always chooses its targets the game's usual way; the target mode of the attacker is chosen outside,
// with the WZ_TARGETMODE environment variable. The attackers attack-move across the base and are then sent after what
// is left of it, but never towards the Hellstorm emplacements, which only artillery can reach.
//
// UNIT lines list everything at the start, BATTLE lines follow the numbers on each side, and with WZ_FIRELOG set the
// game adds a FIRE, LOST or HIT line for every shot and every bit of damage.

const ATTACKER = 0;
const DEFENDER = 1;

const COLS = 6;
const X_LEFT = X_GAP - 3;
const Y_ATTACKER_FRONT = 8;
const Y_GOAL = 25;      // far enough from the cliff that no direct fire weapon reaches past it
const Y_SENSOR_GOAL = 26;   // from here the sensor sees the far Hellstorms through the pass

const PYTHON = "Body9REC";
const TRACKS = "tracked01";

const ATTACKERS = [
	{ n: 3, weap: "MG4ROTARYMk1", label: "Assault Gun" },
	{ n: 3, weap: "Rocket-BB", label: "Bunker Buster" },
	{ n: 3, weap: "Rocket-HvyA-T", label: "Tank Killer" },
	{ n: 3, weap: "Mortar2Mk1", label: "Bombard" },
];
const ATTACKER_SENSOR = { turret: "Sensor-WideSpec", label: "Wide Spectrum Sensor Tank" };
const ATTACKER_HOWITZER = { weap: "Howitzer03-Rot", label: "Hellstorm Tank" };

const DEFENDER_STRUCTURES = [
	{ id: "Sys-SensoTower02", label: "Front Sensor Tower", dx: 0, y: 13 },
	{ id: "Wall-RotMg", label: "Assault Gun Hardpoint", dx: -5, y: 16 },
	{ id: "WallTower-HvATrocket", label: "Tank Killer Hardpoint", dx: 5, y: 16 },
	{ id: "Sys-SensoTower02", label: "Walled Sensor Tower", dx: -5, y: 17 },
	{ id: "PillBox-Cannon6", label: "Twin Assault Cannon Bunker", dx: -1, y: 16 },
	{ id: "PillBox-Cannon6", label: "Twin Assault Cannon Bunker", dx: 1, y: 16 },
	{ id: "Emplacement-MortarPit01", label: "Mortar Pit", dx: -2, y: 22 },
	{ id: "Emplacement-MortarPit01", label: "Mortar Pit", dx: 2, y: 22 },
	// behind the cliff, in line with the two tiles of the pass, so that a sensor on the near side can see them through it
	{ id: "Emplacement-RotHow", label: "Hellstorm Emplacement", dx: -1, y: 37, far: true },
	{ id: "Emplacement-RotHow", label: "Hellstorm Emplacement", dx: 0, y: 37, far: true },
	{ id: "Emplacement-RotHow", label: "Hellstorm Emplacement", dx: -1, y: 39, far: true },
	{ id: "Emplacement-RotHow", label: "Hellstorm Emplacement", dx: 0, y: 39, far: true },
];
// concrete around the walled sensor tower: on both sides and behind it, the hardpoint stands in front
const SENSOR_WALLS = [{ dx: -6, y: 17 }, { dx: -4, y: 17 }, { dx: -6, y: 18 }, { dx: -5, y: 18 }, { dx: -4, y: 18 }];

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
var farIds = [];
var sensorIds = [];

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

function standing(ids)
{
	var count = 0;
	for (var i = 0; i < ids.length; i++)
	{
		if (getObject(STRUCTURE, DEFENDER, ids[i]) !== null)
		{
			count++;
		}
	}
	return count;
}

function structureTally()
{
	var structures = enumStruct(DEFENDER);
	var count = 0;
	var health = 0;
	for (var i = 0; i < structures.length; i++)
	{
		if (structures[i].stattype !== WALL)
		{
			count++;
			health += structures[i].health;
		}
	}
	return count + " health=" + Math.round(health);
}

function battleCheck()
{
	var line = "attacker=" + tally(ATTACKER) + " defender=" + tally(DEFENDER) + " structures=" + structureTally()
	           + " far=" + standing(farIds) + " sensors=" + standing(sensorIds);
	if (line !== lastReport)
	{
		lastReport = line;
		debug("BATTLE t=" + gameTime + " " + line);
	}
}

// what is left standing on both sides, every few seconds, so the last of these shows how things ended
function snapshot()
{
	var mine = enumDroid(ATTACKER);
	var notes = [];
	for (var j = 0; j < mine.length; j++)
	{
		if (mine[j].droidType === DROID_SENSOR || mine[j].name === ATTACKER_HOWITZER.label)
		{
			notes.push(mine[j].name + "@" + mine[j].x + "," + mine[j].y + " action=" + mine[j].action + " order=" + mine[j].order);
		}
	}
	debug("SPOT t=" + gameTime + " " + notes.join(" | "));
	var all = enumDroid(ATTACKER).concat(enumDroid(DEFENDER), enumStruct(DEFENDER));
	var parts = [];
	for (var i = 0; i < all.length; i++)
	{
		parts.push(all[i].id + ":" + Math.round(all[i].health));
	}
	debug("STATE t=" + gameTime + " " + parts.join(" "));
}

// Sends every idle assault tank after the nearest thing left in the base, walls and the far Hellstorms aside.
function sweep()
{
	var attackers = enumDroid(ATTACKER, DROID_WEAPON);
	var targets = enumDroid(DEFENDER);
	var structures = enumStruct(DEFENDER);
	for (var s = 0; s < structures.length; s++)
	{
		if (structures[s].stattype !== WALL && farIds.indexOf(structures[s].id) < 0)
		{
			targets.push(structures[s]);
		}
	}
	// a sensor tank that has stopped short, once it has nothing left to watch, goes on to where it can see past the cliff
	// (25 is DORDER_GUARD and 7 DACTION_OBSERVE, which the scripts have no names for)
	var mine = enumDroid(ATTACKER, DROID_SENSOR);
	for (var m = 0; m < mine.length; m++)
	{
		if ((mine[m].order === 0 || mine[m].order === 25) && mine[m].action !== 7 && mine[m].y < Y_SENSOR_GOAL - 1)
		{
			orderDroidLoc(mine[m], DORDER_SCOUT, X_GAP, Y_SENSOR_GOAL);
		}
	}
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
		// the Hellstorms stay at the back
		if (attackers[k].order === 0 && attackers[k].name !== ATTACKER_HOWITZER.label)   // 0 is DORDER_NONE
		{
			orderDroidLoc(attackers[k], DORDER_SCOUT, best.x, best.y);
		}
	}
}

function placeStructure(id, x, y)
{
	if (!addStructure(id, DEFENDER, x * 128, y * 128))
	{
		return null;
	}
	var placed = enumStruct(DEFENDER);
	for (var q = 0; q < placed.length; q++)
	{
		if (placed[q].x === x && placed[q].y === y)
		{
			return placed[q];
		}
	}
	return null;
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
	makeComponentAvailable(ATTACKER_SENSOR.turret, ATTACKER);
	makeComponentAvailable(ATTACKER_HOWITZER.weap, ATTACKER);
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
		var made = placeStructure(st.id, X_GAP + st.dx, st.y);
		if (made !== null)
		{
			debug("UNIT id=" + made.id + " side=defender kind=structure label=" + st.label);
			if (st.far)
			{
				farIds.push(made.id);
			}
			if (st.id.indexOf("Senso") >= 0)
			{
				sensorIds.push(made.id);
			}
		}
	}
	for (var sw = 0; sw < SENSOR_WALLS.length; sw++)
	{
		placeStructure("A0HardcreteMk1Wall", X_GAP + SENSOR_WALLS[sw].dx, SENSOR_WALLS[sw].y);
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

	// assault tanks in two ranks, the sensor tank behind them, the Hellstorms at the back
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
	var sensor = addDroid(ATTACKER, X_GAP + dxA, Y_ATTACKER_FRONT + dyA - 2, ATTACKER_SENSOR.label, PYTHON, TRACKS, "", "", ATTACKER_SENSOR.turret);
	if (sensor !== null)
	{
		debug("UNIT id=" + sensor.id + " side=attacker kind=sensor label=" + ATTACKER_SENSOR.label);
	}
	var howitzers = [];
	for (var hw = 0; hw < 2; hw++)
	{
		var hd = addDroid(ATTACKER, X_GAP - 2 + 4 * hw + dxA, Y_ATTACKER_FRONT + dyA - 4, ATTACKER_HOWITZER.label, PYTHON, TRACKS, "", "", ATTACKER_HOWITZER.weap);
		if (hd !== null)
		{
			howitzers.push(hd);
			debug("UNIT id=" + hd.id + " side=attacker kind=tank label=" + ATTACKER_HOWITZER.label);
		}
	}
	hackNetOn();

	for (var m = 0; m < attackers.length; m++)
	{
		orderDroidLoc(attackers[m], DORDER_SCOUT, attackers[m].x, Y_GOAL);
	}
	if (sensor !== null)
	{
		orderDroidLoc(sensor, DORDER_SCOUT, sensor.x, Y_SENSOR_GOAL);
	}
	for (var n = 0; n < defenders.length; n++)
	{
		orderDroidLoc(defenders[n], DORDER_SCOUT, defenders[n].x, defenders[n].y);
	}

	setTimer("battleCheck", 500);
	setTimer("sweep", 5000);
	setTimer("snapshot", 5000);
	debug("movebench: precision2, " + (attackers.length + howitzers.length + (sensor ? 1 : 0)) + " attackers against "
	      + defenders.length + " units and " + DEFENDER_STRUCTURES.length + " structures");
}
