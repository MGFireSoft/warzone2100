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

function eventStartLevel()
{
	hackNetOff();
	benchEnable(ME, "heavy");
	var pack = benchSpawnBlock(ME, "heavy", X_GAP - 2, Y_PACK, PACK_COLS, +1, PACK_COLS * PACK_ROWS);
	hackNetOn();

	orderDroidLoc(pack[PACK_CENTRE], DORDER_MOVE, X_GAP, Y_GOAL);

	debug("movebench: packed, 1 of " + pack.length + " heavy tanks ordered out of the middle");
}
