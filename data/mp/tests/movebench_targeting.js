include("tests/movebench_common.js");

// A strong mixed force attack-moves on a weaker mixed one that stands still, to compare target modes.
//
// Player 0 is the attacker: anti-tank missile tanks, machine gun tanks and cannon tanks. Player 1 stands
// still with sixteen cyborgs in front, which are the closest targets, and eight cannon tanks behind them,
// which are the ones doing the real damage and the ones anti-tank weapons are best at. Picking the right
// target first is what decides how fast the cannon tanks die and how many attackers are lost.
//
// The target mode is chosen outside, with the WZ_TARGETMODE environment variable, and applies to every unit
// of player 0. BATTLE lines are printed whenever the number of units left changes.

const STRONG = 0;
const WEAK = 1;

const STRONG_KINDS = ["homing", "medium", "homing", "cannon"];
const N_STRONG = 24;
const N_CYBORGS = 16;
const N_TANKS = 8;
const COLS = 8;

const X_LEFT = X_GAP - 4;
const Y_STRONG = 5;
const Y_WEAK = 22;
const Y_ATTACK = 21;

var lastReport = "";

function battleCheck()
{
	var attackers = enumDroid(STRONG);
	var health = 0;
	for (var i = 0; i < attackers.length; i++)
	{
		health += attackers[i].health;
	}
	var line = "strong=" + attackers.length + " health=" + Math.round(health) + " tanks=" + enumDroid(WEAK, DROID_WEAPON).length + " cyborgs=" + enumDroid(WEAK, DROID_CYBORG).length + " structures=" + enumStruct(WEAK).length;
	if (line !== lastReport)
	{
		lastReport = line;
		debug("BATTLE t=" + gameTime + " " + line);
	}
}

function eventStartLevel()
{
	hackNetOff();
	for (var i = 0; i < STRONG_KINDS.length; i++)
	{
		benchEnable(STRONG, STRONG_KINDS[i]);
	}
	benchEnable(WEAK, "cannon");
	benchEnable(WEAK, "cyborg");

	addStructure("WallTower01", WEAK, (X_GAP - 3) * 128, (Y_WEAK + 3) * 128);
	addStructure("WallTower01", WEAK, (X_GAP + 3) * 128, (Y_WEAK + 3) * 128);

	var strong = benchSpawnMixedBlock(STRONG, STRONG_KINDS, X_LEFT, Y_STRONG, COLS, +1, N_STRONG);
	var defenders = benchSpawnMixedBlock(WEAK, ["cyborg"], X_LEFT, Y_WEAK, COLS, +1, N_CYBORGS);
	defenders = defenders.concat(benchSpawnMixedBlock(WEAK, ["cannon"], X_LEFT, Y_WEAK + 2, COLS, +1, N_TANKS));
	hackNetOn();

	// attack-move onto their own tile: they stand their ground and fire at whatever comes within reach
	for (var d = 0; d < defenders.length; d++)
	{
		orderDroidLoc(defenders[d], DORDER_SCOUT, defenders[d].x, defenders[d].y);
	}

	for (var k = 0; k < strong.length; k++)
	{
		orderDroidLoc(strong[k], DORDER_SCOUT, X_LEFT + (k % COLS), Y_ATTACK);
	}

	setTimer("battleCheck", 500);
	debug("movebench: targeting, " + strong.length + " attackers against " + N_CYBORGS + " cyborgs in front of " + N_TANKS + " cannon tanks");
}
