// Standalone test for src/formationlayout.cpp. Build and run from a Visual Studio developer shell:
//   cl /EHsc /std:c++20 /Isrc tests/formation/formationlayout_test.cpp src/formationlayout.cpp /Fe:formtest.exe && formtest.exe
#include "formationlayout.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <set>
using namespace formation;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void dump(const char *name, const std::vector<Pt> &s)
{
	printf("%s (%zu):", name, s.size());
	for (auto &p : s) printf(" (%d,%d)", p.x, p.y);
	printf("\n");
}

int main()
{
	const int S = 100;
	// counts and no overlaps for every shape
	Spec specs[] = {{Shape::Square, 1}, {Shape::Circle, 1}, {Shape::Line, 2}, {Shape::Line, 3}, {Shape::Column, 1}, {Shape::Column, 2}, {Shape::Column, 3}};
	for (auto spec : specs)
	{
		for (size_t n : {0, 1, 2, 5, 9, 10, 25, 49})
		{
			auto slots = makeSlots(spec, n, S);
			CHECK(slots.size() == n);
			for (size_t i = 0; i < slots.size(); i++)
				for (size_t j = i + 1; j < slots.size(); j++)
				{
					double d = std::hypot(slots[i].x - slots[j].x, slots[i].y - slots[j].y);
					if (d < S * 0.9) { printf("overlap shape %d param %d n %zu: %zu %zu d=%.1f\n", (int)spec.shape, spec.param, n, i, j, d); failures++; }
				}
			for (size_t i = 1; i < slots.size(); i++)
				CHECK(slots[i - 1].y > slots[i].y || (slots[i - 1].y == slots[i].y && slots[i - 1].x < slots[i].x));
		}
	}
	dump("square 7", makeSlots({Shape::Square, 1}, 7, S));
	dump("line d2 7", makeSlots({Shape::Line, 2}, 7, S));
	dump("column w2 7", makeSlots({Shape::Column, 2}, 7, S));
	dump("circle 8", makeSlots({Shape::Circle, 1}, 8, S));

	// shapes: a column of width 1 is one file, a line of depth 1 is one rank
	{
		auto c = makeSlots({Shape::Column, 1}, 4, S);
		CHECK(c[0].x == 0 && c[3].x == 0 && c[0].y > c[3].y);
		auto l = makeSlots({Shape::Line, 1}, 4, S);
		CHECK(l[0].y == l[3].y && l[0].x < l[3].x);
	}

	// sorting criteria, heading north (0): forward is +y
	{
		std::vector<Unit> u(6);
		for (int i = 0; i < 6; i++) u[i].id = i + 1;
		int ranges[] = {900, 300, 600, 1200, 0, 0};
		int tough[] = {10, 50, 30, 5, 20, 40};
		for (int i = 0; i < 6; i++) { u[i].range = ranges[i]; u[i].toughness = tough[i]; u[i].speed = 100 - 10 * i; u[i].pos = {i * 10, 0}; }
		u[3].artillery = true;
		auto slots = makeSlots({Shape::Column, 1}, 6, S);  // a single file, slot 0 is the front
		auto byRange = assign(u, slots, 0, SortBy::Range);
		// artillery (unit index 3) is last, short range first (range 0 units 4 and 5, then 300 = unit 1)
		CHECK(byRange[3] == 5);
		CHECK(byRange[1] == 2);
		CHECK(byRange[0] == 3 || byRange[0] == 4);
		auto byTough = assign(u, slots, 0, SortBy::Toughness);
		CHECK(byTough[3] == 5);   // artillery last even though it is the most fragile
		CHECK(byTough[1] == 0);   // toughest of the rest in front
		auto bySpeed = assign(u, slots, 0, SortBy::Speed);
		CHECK(bySpeed[0] == 0);
		// every unit gets its own place
		std::set<size_t> used(byRange.begin(), byRange.end());
		CHECK(used.size() == 6);
	}

	// no crossing inside a rank: units spread left to right keep that order
	{
		std::vector<Unit> u(3);
		u[0].id = 1; u[0].pos = {300, 0};
		u[1].id = 2; u[1].pos = {-300, 0};
		u[2].id = 3; u[2].pos = {0, 0};
		auto slots = makeSlots({Shape::Line, 1}, 3, S);   // one rank, left to right
		auto a = assign(u, slots, 0, SortBy::Position);   // facing north, right is +x
		CHECK(a[1] == 0 && a[2] == 1 && a[0] == 2);
		// facing south the same units are mirrored
		auto b = assign(u, slots, 32768, SortBy::Position);
		CHECK(b[0] == 0 && b[2] == 1 && b[1] == 2);
	}

	// toWorld: front slot ends up ahead of the centre for each heading
	{
		Pt front = {0, 100};
		Pt n = toWorld(front, {1000, 1000}, 0);       CHECK(n.x == 1000 && n.y == 1100);
		Pt e = toWorld(front, {1000, 1000}, 16384);   CHECK(e.x == 1100 && e.y == 1000);
		Pt s = toWorld(front, {1000, 1000}, 32768);   CHECK(s.x == 1000 && s.y == 900);
		Pt right = {100, 0};
		Pt rn = toWorld(right, {0, 0}, 0);            CHECK(rn.x == 100 && rn.y == 0);
		Pt re = toWorld(right, {0, 0}, 16384);        CHECK(re.x == 0 && re.y == -100);
	}

	printf(failures ? "FAILURES: %d\n" : "all passed\n", failures);
	return failures != 0;
}
