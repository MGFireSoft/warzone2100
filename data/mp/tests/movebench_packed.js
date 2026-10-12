include("tests/movebench_common.js");

// A solid block of heavy tanks, all idle and touching, with one ordered out of the middle.
//
// Exercises moveShuffleDroid against a pack with no free side: the ordered unit is walled in on
// every side by allies sitting at MOVEINACTIVE, so it can only leave if they make room in a chain.
// Only the centre unit is given an order, so unitsOrdered counts just that one and its arrival
// time is the result. Packed shoulder to shoulder, a heavy hull (radius 60) leaves 8 units of slack
// on a 128-unit tile.

const ME = 0;
const PACK_COLS = 5;
const PACK_ROWS = 5;
const PACK_CENTRE = 12;   // index of the middle unit in a 5x5 row-major block

const Y_PACK = 10;
const Y_GOAL = 28;

var packUnits = [];
var packStart = [];
const REPORT_MS = 85000;

// How many of the idle units that were pushed aside have gone back to the tile they started on.
function packReport()
{
	var home = 0;
	var away = 0;
	for (var i = 0; i < packUnits.length; i++)
	{
		if (i === PACK_CENTRE)
		{
			continue;
		}
		if (packUnits[i].x === packStart[i].x && packUnits[i].y === packStart[i].y)
		{
			home++;
		}
		else
		{
			away++;
		}
	}
	debug("PACKREPORT idle units back on their tile: " + home + ", still displaced: " + away);
}

function eventStartLevel()
{
	hackNetOff();
	benchEnable(ME, "heavy");
	var pack = benchSpawnBlock(ME, "heavy", X_GAP - 2, Y_PACK, PACK_COLS, +1, PACK_COLS * PACK_ROWS);
	hackNetOn();

	packUnits = pack;
	for (var i = 0; i < pack.length; i++)
	{
		packStart.push({ x: pack[i].x, y: pack[i].y });
	}
	queue("packReport", REPORT_MS);

	orderDroidLoc(pack[PACK_CENTRE], DORDER_MOVE, X_GAP, Y_GOAL);

	debug("movebench: packed, 1 of " + pack.length + " heavy tanks ordered out of the middle");
}
