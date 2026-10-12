// Standalone test for src/targetweights.cpp. Build and run from a Visual Studio developer shell:
//   cl /EHsc /std:c++20 /Isrc tests/formation/targetweights_test.cpp src/targetweights.cpp /Fe:targettest.exe && targettest.exe
#include "targetweights.h"
#include <cstdio>
#include <initializer_list>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

// a target `tiles` away from an attacker with 8 tiles of sensor range
static TargetTerms terms(int fit, int tiles, int hitpoints, bool structure = false, int damagePct = 0, int typeBonus = 52)
{
	TargetTerms t;
	t.fit = fit;
	t.distance = 13 * 8 - 13 * tiles;
	t.health = 130 * damagePct / 100;
	t.typeBonus = typeBonus;
	t.hitpoints = hitpoints;
	t.isStructure = structure;
	return t;
}

int main()
{
	// an anti-tank weapon (30% against cyborgs, 120% against tracked) with a cyborg close and a tank further away
	const TargetTerms cyborgNear = terms(30, 1, 300);
	const TargetTerms tankFar = terms(120, 6, 600);
	CHECK(targetModeWeight(TargetMode::ByType, tankFar) > targetModeWeight(TargetMode::ByType, cyborgNear));
	CHECK(targetModeWeight(TargetMode::Smart, tankFar) > targetModeWeight(TargetMode::Smart, cyborgNear));
	// a tank at the very edge of its reach against a cyborg at point blank: by type still goes for the tank
	CHECK(targetModeWeight(TargetMode::ByType, terms(120, 8, 600)) > targetModeWeight(TargetMode::ByType, terms(30, 0, 300)));

	// a machine gun (150% against cyborgs, 40% against tracked)
	const TargetTerms cyborgMg = terms(150, 5, 300);
	const TargetTerms tankMg = terms(40, 2, 600);
	CHECK(targetModeWeight(TargetMode::ByType, cyborgMg) > targetModeWeight(TargetMode::ByType, tankMg));
	CHECK(targetModeWeight(TargetMode::Smart, cyborgMg) > targetModeWeight(TargetMode::Smart, tankMg));

	// nearest ignores everything else
	CHECK(targetModeWeight(TargetMode::Nearest, terms(30, 1, 900)) > targetModeWeight(TargetMode::Nearest, terms(150, 3, 10)));

	// weakest takes the one with fewer hitpoints left, all else equal
	CHECK(targetModeWeight(TargetMode::Weakest, terms(100, 4, 80)) > targetModeWeight(TargetMode::Weakest, terms(100, 4, 800)));
	// and it still does when the weak one is a bit further
	CHECK(targetModeWeight(TargetMode::Weakest, terms(100, 6, 80)) > targetModeWeight(TargetMode::Weakest, terms(100, 4, 800)));

	// kinds
	CHECK(targetModeWeight(TargetMode::Structures, terms(100, 6, 500, true)) > targetModeWeight(TargetMode::Structures, terms(100, 2, 500, false)));
	CHECK(targetModeWeight(TargetMode::Units, terms(100, 6, 500, false)) > targetModeWeight(TargetMode::Units, terms(100, 2, 500, true)));
	CHECK(targetModeWeight(TargetMode::Default, terms(100, 6, 500, true)) < targetModeWeight(TargetMode::Default, terms(100, 2, 500, false)));

	// smart: among targets the weapon is good against, the weaker goes first
	CHECK(targetModeWeight(TargetMode::Smart, terms(120, 4, 100)) > targetModeWeight(TargetMode::Smart, terms(120, 4, 900)));
	// but a poor fit loses even to a healthier, further target it is good against
	CHECK(targetModeWeight(TargetMode::Smart, terms(120, 6, 900)) > targetModeWeight(TargetMode::Smart, terms(30, 2, 50)));

	// every mode gives a positive weight for plausible targets, the game treats zero as no target
	for (int m = 0; m < static_cast<int>(TargetMode::Count); m++)
	{
		for (int fit : {30, 100, 150})
		{
			for (int tiles : {0, 4, 9})
			{
				CHECK(targetModeWeight(static_cast<TargetMode>(m), terms(fit, tiles, 300)) > 0);
			}
		}
		CHECK(targetModeName(static_cast<TargetMode>(m))[0] != '\0');
	}

	printf(failures ? "FAILURES: %d\n" : "all passed\n", failures);
	return failures != 0;
}
