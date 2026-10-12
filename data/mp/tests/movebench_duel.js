include("tests/movebench_common.js");

// Two identical mixed forces facing each other, to compare how well each target mode fights.
//
// Both sides field the same 24 units, with many weapons, bodies and propulsions, laid out by weapon range:
// short range in the front rank, the mortars at the back. The defender stands its ground and always picks
// targets the game's usual way, and also has two towers. The attacker has two extra tanks instead of those
// towers and advances with an attack-move. The target mode of the attacker is chosen outside, with the
// WZ_TARGETMODE environment variable, and applies to every unit of player 0.
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

// weakest reach first, which is the order they take their places in, front rank first
const FORCE = [
	{ n: 2, body: "cyborg", prop: "legs", weap: "CyborgFlamer01" },
	{ n: 1, body: "cobra", prop: "tracks", weap: "Flame1Mk1" },
	{ n: 2, body: "viper", prop: "wheels", weap: "MG1Mk1" },
	{ n: 1, body: "cobra", prop: "hover", weap: "MG1Mk1" },
	{ n: 2, body: "cyborg", prop: "legs", weap: "CyborgChaingun" },
	{ n: 2, body: "cobra", prop: "half", weap: "MG2Mk1" },
	{ n: 2, body: "python", prop: "tracks", weap: "MG3Mk1" },
	{ n: 2, body: "python", prop: "tracks", weap: "Cannon1Mk1" },
	{ n: 1, body: "viper", prop: "half", weap: "Cannon1Mk1" },
	{ n: 2, body: "cyborg", prop: "legs", weap: "CyborgRocket" },
	{ n: 1, body: "cobra", prop: "tracks", weap: "Cannon2A-TMk1" },
	{ n: 2, body: "cobra", prop: "half", weap: "Rocket-LtA-T" },
	{ n: 1, body: "cobra", prop: "half", weap: "Rocket-MRL" },
	{ n: 1, body: "python", prop: "tracks", weap: "Missile-A-T" },
	{ n: 2, body: "cobra", prop: "half", weap: "Mortar1Mk1" },
];

// stand in for the defender's two towers
const EXTRA_TANKS = [
	{ n: 1, body: "python", prop: "tracks", weap: "MG3Mk1" },
	{ n: 1, body: "python", prop: "tracks", weap: "Rocket-LtA-T" },
];

var lastReport = "";

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

// Lays the force out front rank first. Ranks recede in rankDir, which points away from the enemy.
function spawnForce(player, x0, yFront, rankDir)
{
	var made = [];
	var index = 0;
	for (var g = 0; g < FORCE.length; g++)
	{
		for (var k = 0; k < FORCE[g].n; k++)
		{
			var x = x0 + (index % COLS);
			var y = yFront + Math.floor(index / COLS) * rankDir;
			var d = addDroid(player, x, y, "Duel " + g + "-" + k, BODY[FORCE[g].body], PROP[FORCE[g].prop], "", "", FORCE[g].weap);
			if (d !== null)
			{
				made.push(d);
			}
			index++;
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

	hackNetOff();
	enableAll(ATTACKER, FORCE);
	enableAll(ATTACKER, EXTRA_TANKS);
	enableAll(DEFENDER, FORCE);

	addStructure("WallTower01", DEFENDER, (X_GAP - 3 + dxD) * 128, (Y_DEFENDER_FRONT - 1) * 128);
	addStructure("WallTower06", DEFENDER, (X_GAP + 2 + dxD) * 128, (Y_DEFENDER_FRONT - 1) * 128);

	var attackers = spawnForce(ATTACKER, X_LEFT + dxA, Y_ATTACKER_FRONT, -1);
	var defenders = spawnForce(DEFENDER, X_LEFT + dxD, Y_DEFENDER_FRONT, +1);
	for (var e = 0; e < EXTRA_TANKS.length; e++)
	{
		var t = EXTRA_TANKS[e];
		var d = addDroid(ATTACKER, X_GAP - 3 + 5 * e + dxA, Y_ATTACKER_FRONT + 1, "Duel extra " + e, BODY[t.body], PROP[t.prop], "", "", t.weap);
		if (d !== null)
		{
			attackers.push(d);
		}
	}
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
	debug("movebench: duel, " + attackers.length + " attackers against " + defenders.length + " defenders and 2 towers");
}
