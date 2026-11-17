// SPDX-License-Identifier: GPL-3.0-or-later
//
// The entry point (GUIDE.md section 4): it hands the load to the part for the running game. Only
// Ratchet & Clank (rac1) has one, src/rac1/game.cpp; any other game is refused with a reason.
#include "openrac/mod.hpp"

#include <string>

#include "rac1/game.h"
#include "version.h"

namespace {

// Loads rac1's part, or refuses any other game. Called once by the port at start, on the game's
// thread.
void load(openrac::Mod& mod) {
    // Ratchet & Clank is the only game with a part.
    if (mod.game() == "rac1") {
        travel_anywhere::rac1::load(mod);
        return;
    }

    throw openrac::Error(
        "Travel anywhere only works with Ratchet & Clank (rac1), not " + std::string(mod.game())
    );
}

}  // namespace

OPENRAC_MOD_CPP(RAC1_TRAVEL_ANYWHERE_ID, RAC1_TRAVEL_ANYWHERE_VERSION, load)
